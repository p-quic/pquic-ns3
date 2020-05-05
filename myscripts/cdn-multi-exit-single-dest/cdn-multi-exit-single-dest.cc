#include <ns3/bulk-send-helper.h>
#include <ns3/network-module.h>
#include <ns3/packet-sink-helper.h>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "../helper/quic-iperf-network-simulator-helper.h"
#include "../helper/quic-point-to-point-helper.h"

#ifdef USE_MPI
#include "ns3/mpi-interface.h"
#endif

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ns3 simulator");

void change_links(std::vector<QuicPointToPointHelper*> links, std::vector<std::string> bws, std::vector<std::string> delays, std::vector<std::string> queues) {
    for (int i = 0; i < links.size(); i++) {
        Config::Set ("/NodeList/0/DeviceList/" + std::to_string(i + 1) + "/$ns3::PointToPointNetDevice/DataRate", StringValue (bws[i]));
        Config::Set ("/NodeList/0/DeviceList/" + std::to_string(i + 1) + "/$ns3::PointToPointChannel/Delay", StringValue (delays[i]));
        Config::Set ("/NodeList/1/DeviceList/" + std::to_string(i + 1) + "/$ns3::PointToPointNetDevice/DataRate", StringValue (bws[i]));
        Config::Set ("/NodeList/1/DeviceList/" + std::to_string(i + 1) + "/$ns3::PointToPointChannel/Delay", StringValue (delays[i]));
    }
}

int main(int argc, char *argv[]) {
  std::string topo, flows, ecmp;
  CommandLine cmd;
  cmd.AddValue("topo", "topo file", topo);
  cmd.AddValue("flows", "flows file", flows);
  cmd.AddValue("ecmp", "use ecmp", ecmp);
  cmd.Parse (argc, argv);

  NS_ABORT_MSG_IF(topo.length() == 0, "Missing parameter: topo");
  NS_ABORT_MSG_IF(flows.length() == 0, "Missing parameter: flows");

#ifdef USE_MPI
  MpiInterface::Enable (&argc, &argv);
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::DistributedSimulatorImpl"));

  uint32_t systemId = MpiInterface::GetSystemId ();
  uint32_t systemCount = MpiInterface::GetSize ();
#else
    uint32_t systemId = 0;
    uint32_t systemCount = 0;
#endif

  int nlinks = 0;
  {
    std::ifstream topoFile(topo);
    std::string line;
    while (topoFile >> line && line.rfind('#', 0) != 0) {
        nlinks++;
    }
  }

  if (ecmp.length() > 0) {
      setenv("PQUIC_ECMP", std::to_string(nlinks).c_str(), 1);
  }

  std::ifstream flowsFile(flows);

  std::vector<std::string> durations;
  std::vector<std::string> bandwidths;
  std::string line;
  while (flowsFile >> line) {
      std::string duration, bandwidth;

      std::stringstream ss(line);
      char delim = ',';
      std::getline(ss, duration, delim);
      std::getline(ss, bandwidth, delim);
      durations.push_back(duration);
      bandwidths.push_back(bandwidth);
  }

#ifdef USE_MPI
  if (systemCount != durations.size() * 2) {
      printf("The simulation needs %lu MPI processes\n", durations.size() * 2);
      MpiInterface::Disable ();
      return 0;
  }
#endif

  QuicIperfNetworkSimulatorHelper sim = QuicIperfNetworkSimulatorHelper(durations, bandwidths, systemId);

  uint16_t port = 9;   // Discard port (RFC 863)
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
  // use a large receive buffer, so that we don't become flow control blocked
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(10 * 1024 * 1024));
  // use a very large send buffer, otherwise ns3 will create sub-MTU size packets
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(100 * 1024 * 1024));
  Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true));
  Config::SetDefault("ns3::TcpSocketBase::EcnMode", StringValue ("ClassicEcn"));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  // Stick in N point-to-point lines between the sides.
  std::ifstream topoFile(topo);
  int i = 0;
  std::vector<QuicPointToPointHelper*> links;
  while (topoFile >> line && line.rfind('#', 0) != 0) {
      std::string bandwidth, delay, queue, tcp_flows;

      std::stringstream ss(line);
      char delim = ',';
      std::getline(ss, bandwidth, delim);
      std::getline(ss, delay, delim);
      std::getline(ss, queue, delim);
      std::getline(ss, tcp_flows, delim);

      printf("Link: %s, %s, %s, with %s TCP flows\n", bandwidth.c_str(), delay.c_str(), queue.c_str(),
             tcp_flows.c_str());

      QuicPointToPointHelper p2p;
      p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
      p2p.SetChannelAttribute("Delay", StringValue(delay));
      p2p.SetQueueSize(StringValue(queue + "p"));
      p2p.SetBandwidth(StringValue(bandwidth));
      p2p.SetDelay(StringValue(delay));
      links.push_back(&p2p);

      NetDeviceContainer devices = p2p.Install(sim.GetLeftNode(), sim.GetRightNode(), "ns3::FqCoDelQueueDisc");
      Ipv4AddressHelper ipv4;
      char base_ipv4[17];
      sprintf(base_ipv4, "192.168.%d.0", 50 + i);
      ipv4.SetBase(base_ipv4, "255.255.255.0");
      Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
      i++;

      for (int fi = 0; fi < atoi(tcp_flows.c_str()); fi++) {
          BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
          source.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
          ApplicationContainer source_apps = source.Install(sim.GetRightNode());
          source_apps.Start(Seconds(0));
      }

      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
      ApplicationContainer apps = sink.Install(sim.GetLeftNode());
      apps.Start(Seconds(0));
  }

  while (line.rfind('#', 0) == 0) {
      printf("Topo change at %s\n", line.c_str());
      int event_time = atoi(line.substr(1).c_str());
      std::vector<std::string> bws, delays, queues;

      while (topoFile >> line && line.rfind('#', 0) != 0) {
          std::string bandwidth, delay, queue;
          std::stringstream ss(line);
          char delim = ',';
          std::getline(ss, bandwidth, delim);
          std::getline(ss, delay, delim);
          std::getline(ss, queue, delim);
          bws.push_back(bandwidth);
          delays.push_back(delay);
          queues.push_back(queue);

          printf("Link: %s, %s, %s\n", bandwidth.c_str(), delay.c_str(), queue.c_str());
      }

      Simulator::Schedule(Seconds(event_time), &change_links, links, bws, delays, queues);
  }

  sim.Run(Seconds(180));
#ifdef USE_MPI
  // Exit the MPI execution environment
  MpiInterface::Disable ();
#endif
}
