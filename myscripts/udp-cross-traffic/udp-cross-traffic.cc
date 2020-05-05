#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "../helper/quic-network-simulator-helper.h"
#include "../helper/quic-point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ns3 simulator");

int main(int argc, char *argv[]) {
  std::string delay, bandwidth, queue, cross_data_rate, filesize;
  CommandLine cmd;
  cmd.AddValue("delay", "delay of the p2p link", delay);
  cmd.AddValue("bandwidth", "bandwidth of the p2p link", bandwidth);
  cmd.AddValue("queue", "queue size of the p2p link (in packets)", queue);
  cmd.AddValue("filesize", "filesize to request (in bytes)", filesize);
  cmd.AddValue("crossdatarate", "data rate of the cross traffic", cross_data_rate);
  cmd.Parse (argc, argv);

  NS_ABORT_MSG_IF(delay.length() == 0, "Missing parameter: delay");
  NS_ABORT_MSG_IF(bandwidth.length() == 0, "Missing parameter: bandwidth");
  NS_ABORT_MSG_IF(queue.length() == 0, "Missing parameter: queue");
  NS_ABORT_MSG_IF(filesize.length() == 0, "Missing parameter: filesize");
  NS_ABORT_MSG_IF(cross_data_rate.length() == 0, "Missing parameter: crossdatarate");

  std::vector<std::string> filesizes;
  filesizes.push_back(filesize);
  QuicNetworkSimulatorHelper sim = QuicNetworkSimulatorHelper(filesizes);

  QuicPointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  p2p.SetChannelAttribute("Delay", StringValue(delay));
  p2p.SetQueueSize(StringValue(queue + "p"));

  NetDeviceContainer devices = p2p.Install(sim.GetLeftNode(), sim.GetRightNode(), "ns3::PfifoFastQueueDisc");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.50.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  uint16_t port = 9;   // Discard port (RFC 863)
  // Create a sink to receive the packets on the left node.
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  sink.Install(sim.GetLeftNode()).Start(Seconds(20));

  // Create a UDP packet source on the right node.
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
  onoff.SetConstantRate(DataRate(cross_data_rate));
  onoff.Install(sim.GetRightNode()).Start(Seconds(20));

  sim.Run(Seconds(36000));
}
