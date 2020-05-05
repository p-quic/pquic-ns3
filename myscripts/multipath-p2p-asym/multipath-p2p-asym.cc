#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "../helper/quic-network-simulator-helper.h"
#include "../helper/quic-point-to-point-helper.h"
#include "../helper/droplist-error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ns3 simulator");

int main(int argc, char *argv[]) {
  std::string delay, delay_bal, queue, bandwidth, bandwidth_bal, filesize, c0d, c1d, s0d, s1d;
  Ptr<DroplistErrorModel> client0_drops = CreateObject<DroplistErrorModel>();
  Ptr<DroplistErrorModel> client1_drops = CreateObject<DroplistErrorModel>();
  Ptr<DroplistErrorModel> server0_drops = CreateObject<DroplistErrorModel>();
  Ptr<DroplistErrorModel> server1_drops = CreateObject<DroplistErrorModel>();
  CommandLine cmd;

  cmd.AddValue("delay", "average delay of the p2p links", delay);
  cmd.AddValue("bandwidth", "sum of bandwidth of the p2p links", bandwidth);
  cmd.AddValue("queue", "unused", queue);
  cmd.AddValue("delay_balance", "percentage of delay balance", delay_bal);
  cmd.AddValue("bandwidth_balance", "percentage of bandwidth balance", bandwidth_bal);
  cmd.AddValue("drops_to_client0", "list of packets (towards client link 0) to drop", c0d);
  cmd.AddValue("drops_to_client1", "list of packets (towards client link 1) to drop", c1d);
  cmd.AddValue("drops_to_server0", "list of packets (towards server link 0) to drop", s0d);
  cmd.AddValue("drops_to_server1", "list of packets (towards server link 1) to drop", s1d);
  cmd.AddValue("filesize", "filesize to request (in bytes)", filesize);
  cmd.Parse (argc, argv);

  NS_ABORT_MSG_IF(delay.length() == 0, "Missing parameter: delay");
  NS_ABORT_MSG_IF(bandwidth.length() == 0, "Missing parameter: bandwidth");
  NS_ABORT_MSG_IF(filesize.length() == 0, "Missing parameter: filesize");
  if (delay_bal.length() == 0) {
      delay_bal = "0.5";
  }
  if (bandwidth_bal.length() == 0) {
      bandwidth_bal = "0.5";
  }

  double b_bal_val = atof(bandwidth_bal.c_str());
  double d_bal_val = atof(delay_bal.c_str());
  double sum_bandwidth = atof(bandwidth.c_str());
  double avg_delay = atof(delay.c_str());
  int queue_val = atoi(queue.c_str());

  SetDrops(client0_drops, c0d);
  SetDrops(client1_drops, c1d);
  SetDrops(server0_drops, s0d);
  SetDrops(server1_drops, s1d);

  std::vector<std::string> filesizes;
  filesizes.push_back(filesize);
  QuicNetworkSimulatorHelper sim = QuicNetworkSimulatorHelper(filesizes);

  for (int i = 0; i < 2; i++) {
      double b = (sum_bandwidth * fabs(i - b_bal_val));
      double d = 2 * avg_delay * fabs(i - d_bal_val);
      int q = 1.5 * (b / 8) * 1024 * 1024 * (2 * d / 1000) / 1200;

      std::stringstream fmt_b;
      fmt_b << b << "Mbps";
      std::stringstream fmt_d;
      fmt_d << d << "ms";
      std::stringstream fmt_q;
      fmt_q << q << "p";

      printf("Link %d: %s %s %s\n", i, fmt_b.str().c_str(), fmt_d.str().c_str(), fmt_q.str().c_str());

      QuicPointToPointHelper p2p;
      p2p.SetDeviceAttribute("DataRate", StringValue(fmt_b.str()));
      p2p.SetChannelAttribute("Delay", StringValue(fmt_d.str()));
      p2p.SetQueueSize(StringValue(fmt_q.str()));

      NetDeviceContainer devices = p2p.Install(sim.GetLeftNode(), sim.GetRightNode(), "ns3::PfifoFastQueueDisc");
      Ipv4AddressHelper ipv4;
      char base_ipv4[17];
      sprintf(base_ipv4, "192.168.%d.0", 50 + i);
      ipv4.SetBase(base_ipv4, "255.255.255.0");
      Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

      devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(i == 0 ? client0_drops : client1_drops));
      devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(i == 0 ? server0_drops : server1_drops));
  }

  sim.Run(Seconds(180));
}
