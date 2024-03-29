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
#include "quic-network-simulator-helper.h"

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

QuicNetworkSimulatorHelper::QuicNetworkSimulatorHelper(std::string filesize) {
  NodeContainer nodes;
  nodes.Create(2);
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

  dce.SetBinary("picoquicdemo");
  dce.ResetArguments();
  dce.ResetEnvironment();
  if (!debug) {
      dce.AddArgument("-l");
      dce.AddArgument("/dev/null");
  }
  if (qlog) {
      dce.AddArgument("-q");
      dce.AddArgument("server.qlog");
  }
  for (size_t i = 0; i < plugins.size(); i++) {
      dce.AddArgument("-P");
      dce.AddArgument(plugins[i]);
  }

  apps = dce.Install(right_node_);
  apps.Start(Seconds(1.0));

  dce.SetBinary("picoquicdemo");
  dce.ResetArguments();
  dce.ResetEnvironment();
  if (!debug) {
      dce.AddArgument("-l");
      dce.AddArgument("/dev/null");
  }
  if (qlog) {
      dce.AddArgument("-q");
      dce.AddArgument("client.qlog");
  }
  for (size_t i = 0; i < plugins.size(); i++) {
      dce.AddArgument("-P");
      dce.AddArgument(plugins[i]);
  }
  dce.AddArgument("-4");
  dce.AddArgument("-G");
  dce.AddArgument(filesize);
  dce.AddArgument("192.168.50.2");
  dce.AddArgument("4443");

  apps = dce.Install(left_node_);
  apps.Start(Seconds(2.0));
}

void QuicNetworkSimulatorHelper::Run(Time duration) {
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  // write the routing table to file
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("dynamic-global-routing.routes", std::ios::out);
  Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(0.), routingStream);

  Simulator::Stop(duration);
  Simulator::Run();
  Simulator::Destroy();
}

Ptr<Node> QuicNetworkSimulatorHelper::GetLeftNode() const {
  return left_node_;
}

Ptr<Node> QuicNetworkSimulatorHelper::GetRightNode() const {
  return right_node_;
}
