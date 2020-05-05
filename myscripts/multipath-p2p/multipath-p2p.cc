#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "../helper/quic-network-simulator-helper.h"
#include "../helper/quic-point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ns3 simulator");

int main(int argc, char *argv[]) {
  std::string delay, bandwidth, queue, filesize, paths;
  CommandLine cmd;
  cmd.AddValue("delay", "delay of the p2p link", delay);
  cmd.AddValue("bandwidth", "bandwidth of the p2p link", bandwidth);
  cmd.AddValue("queue", "queue size of the p2p link (in packets)", queue);
  cmd.AddValue("filesize", "filesize to request (in bytes)", filesize);
  cmd.AddValue("paths", "paths to create", paths);
  cmd.Parse (argc, argv);

  NS_ABORT_MSG_IF(delay.length() == 0, "Missing parameter: delay");
  NS_ABORT_MSG_IF(bandwidth.length() == 0, "Missing parameter: bandwidth");
  NS_ABORT_MSG_IF(queue.length() == 0, "Missing parameter: queue");
  NS_ABORT_MSG_IF(filesize.length() == 0, "Missing parameter: filesize");
  NS_ABORT_MSG_IF(paths.length() == 0, "Missing parameter: paths");

  std::vector<std::string> filesizes;
  filesizes.push_back(filesize);
  QuicNetworkSimulatorHelper sim = QuicNetworkSimulatorHelper(filesizes);

  // Stick in N point-to-point lines between the sides.
  for (int i = 0; i < atoi(paths.c_str()); i++) {
      QuicPointToPointHelper p2p;
      p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
      p2p.SetChannelAttribute("Delay", StringValue(delay));
      p2p.SetQueueSize(StringValue(queue + "p"));

      NetDeviceContainer devices = p2p.Install(sim.GetLeftNode(), sim.GetRightNode(), "ns3::PfifoFastQueueDisc");
      Ipv4AddressHelper ipv4;
      char base_ipv4[17];
      sprintf(base_ipv4, "192.168.%d.0", 50 + i);
      ipv4.SetBase(base_ipv4, "255.255.255.0");
      Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
  }

  sim.Run(Seconds(180));
}
