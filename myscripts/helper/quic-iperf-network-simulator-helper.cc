#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ns3/core-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "quic-iperf-network-simulator-helper.h"

using namespace ns3;

std::vector <std::string> split(std::string str, std::string sep) {
    char *cstr = const_cast<char *>(str.c_str());
    char *current;
    std::vector <std::string> arr;
    current = strtok(cstr, sep.c_str());
    while (current != NULL) {
        arr.push_back(current);
        current = strtok(NULL, sep.c_str());
    }
    return arr;
}

void installNetDevice(Ptr<Node> node, std::string deviceName, Mac48AddressValue macAddress, Ipv4InterfaceAddress ipv4Address) {
  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName(deviceName);
  NetDeviceContainer devices = emu.Install(node);
  Ptr<NetDevice> device = devices.Get(0);
  device->SetAttribute("Address", macAddress);

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  uint32_t interface = ipv4->AddInterface(device);
  ipv4->AddAddress(interface, ipv4Address);
  ipv4->SetMetric(interface, 1);
  ipv4->SetUp(interface);
}

QuicIperfNetworkSimulatorHelper::QuicIperfNetworkSimulatorHelper(std::vector<std::string> durations, std::vector<std::string> bandwidths, uint32_t systemId) {
  NodeContainer nodes;
#ifdef USE_MPI
  uint32_t server_rank = systemId > durations.size() ? systemId - durations.size() : systemId;
  uint32_t client_rank = systemId < durations.size() ? systemId + durations.size() : systemId;
  Ptr<Node> node1 = CreateObject<Node> (client_rank);
  Ptr<Node> node2 = CreateObject<Node> (server_rank);
  nodes.Add (node1);
  nodes.Add (node2);
#else
  nodes.Create(2);
#endif

  InternetStackHelper internet;
  Ipv4DceRoutingHelper routing = Ipv4DceRoutingHelper();
  internet.SetRoutingHelper(routing);
  internet.Install(nodes);

  left_node_ = nodes.Get(0);
  right_node_ = nodes.Get(1);

  DceManagerHelper dceManager;
  dceManager.Install(nodes);

  DceApplicationHelper dce;
  ApplicationContainer apps;
  dce.SetStackSize(1 << 20);

  bool debug = std::getenv("PQUIC_DEBUG") && strlen(std::getenv("PQUIC_DEBUG"));

  std::vector<std::string> plugins;
  if (std::getenv("PQUIC_PLUGINS") && strlen(std::getenv("PQUIC_PLUGINS"))) {
      plugins = split(std::getenv("PQUIC_PLUGINS"), ",");
  }

  bool qlog = std::getenv("PQUIC_QLOG") && strlen(std::getenv("PQUIC_QLOG"));
  int server_port = 4443;

  int links = 1;
  if (std::getenv("PQUIC_ECMP")) {
      links = atoi(std::getenv("PQUIC_ECMP"));
  }

  for (int i = 0; i < durations.size(); i++) {
#ifdef USE_MPI
      if (i != systemId) {
          continue;
      }
#endif

      dce.SetBinary("picoquiciperf");
      dce.ResetArguments();
      dce.ResetEnvironment();
      dce.AddArgument("-1");
      dce.AddArgument("-p");
      dce.AddArgument(std::to_string(server_port + i));
      dce.AddArgument("-J");
      std::string iperfFile = "iperf_" + std::to_string(i) + ".json";
      dce.AddArgument(iperfFile);

      if (!debug) {
          dce.AddArgument("-l");
          dce.AddArgument("/dev/null");
      }
      if (qlog) {
          dce.AddArgument("-q");
          std::string qlogFile = "server_" + std::to_string(i) + ".qlog";
          dce.AddArgument(qlogFile);
      }
      for (size_t j = 0; j < plugins.size(); j++) {
          dce.AddArgument("-P");
          dce.AddArgument(plugins[j]);
      }

      apps = dce.Install(right_node_);
      apps.Start(Seconds(1.0));
  }

  for (int i = 0; i < durations.size(); i++) {
#ifdef USE_MPI
      if ((i + durations.size()) != systemId) {
          continue;
      }
#endif

      dce.SetBinary("picoquiciperf");
      dce.ResetArguments();
      dce.ResetEnvironment();
      dce.AddArgument("-t");
      dce.AddArgument(durations[i]);
      dce.AddArgument("-B");
      dce.AddArgument(bandwidths[i]);

      if (!debug) {
          dce.AddArgument("-l");
          dce.AddArgument("/dev/null");
      }
      if (qlog) {
          dce.AddArgument("-q");
          std::string qlogFile = "client_" + std::to_string(i) + ".qlog";
          dce.AddArgument(qlogFile);
      }
      for (size_t j = 0; j < plugins.size(); j++) {
          dce.AddArgument("-P");
          dce.AddArgument(plugins[j]);
      }

      int dest_subnet = (50 + (i % links));
      std::string dest = "192.168." + std::to_string(dest_subnet) + ".2";

      printf("Client %d connecting to %s for %s secs\n", i, dest.c_str(), durations[i].c_str());

      dce.AddArgument(dest);
      dce.AddArgument(std::to_string(server_port + i));

      apps = dce.Install(left_node_);
      apps.Start(Seconds(2.0 + (i * (3 / durations.size()))));
  }
}

void QuicIperfNetworkSimulatorHelper::Run(Time duration) {
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  // write the routing table to file
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("dynamic-global-routing.routes", std::ios::out);
  Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(0.), routingStream);

  Simulator::Stop(duration);
  Simulator::Run();
  Simulator::Destroy();
}

Ptr<Node> QuicIperfNetworkSimulatorHelper::GetLeftNode() const {
  return left_node_;
}

Ptr<Node> QuicIperfNetworkSimulatorHelper::GetRightNode() const {
  return right_node_;
}
