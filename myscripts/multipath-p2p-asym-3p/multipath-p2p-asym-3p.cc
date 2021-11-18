#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "../helper/quic-network-simulator-helper.h"
#include "../helper/quic-point-to-point-helper.h"
#include "../helper/droplist-error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ns3 simulator");

int main(int argc, char *argv[]) {
  std::string delay, dw0, dw1, dw2, queue, bandwidth, bw0, bw1, bw2, filesize, c0d, c1d, c2d, s0d, s1d, s2d;
  double bw_vals[3], dw_vals[3];
  Ptr<DroplistErrorModel> client_drops[3];
  Ptr<DroplistErrorModel> server_drops[3];

  for (int i = 0; i < 3; i++) {
      client_drops[i] = CreateObject<DroplistErrorModel>();
      server_drops[i] = CreateObject<DroplistErrorModel>();
  }

  CommandLine cmd;

  cmd.AddValue("delay", "total delay spread between p2p links", delay);
  cmd.AddValue("bandwidth", "sum of bandwidth of the p2p links", bandwidth);
  cmd.AddValue("queue", "unused", queue);
  cmd.AddValue("delay_weight_0", "weight of the overall delay on path 0", dw0);
  cmd.AddValue("delay_weight_1", "weight of the overall delay on path 1", dw1);
  cmd.AddValue("delay_weight_2", "weight of the overall delay on path 2", dw2);
  cmd.AddValue("bandwidth_weight_0", "weight of the overall bandwidth on path 0", bw0);
  cmd.AddValue("bandwidth_weight_1", "weight of the overall bandwidth on path 1", bw1);
  cmd.AddValue("bandwidth_weight_2", "weight of the overall bandwidth on path 2", bw2);
  cmd.AddValue("drops_to_client0", "list of packets (towards client link 0) to drop", c0d);
  cmd.AddValue("drops_to_client1", "list of packets (towards client link 1) to drop", c1d);
  cmd.AddValue("drops_to_client1", "list of packets (towards client link 2) to drop", c2d);
  cmd.AddValue("drops_to_server0", "list of packets (towards server link 0) to drop", s0d);
  cmd.AddValue("drops_to_server1", "list of packets (towards server link 1) to drop", s1d);
  cmd.AddValue("drops_to_server2", "list of packets (towards server link 2) to drop", s2d);
  cmd.AddValue("filesize", "filesize to request (in bytes)", filesize);
  cmd.Parse (argc, argv);

  NS_ABORT_MSG_IF(delay.length() == 0, "Missing parameter: delay");
  NS_ABORT_MSG_IF(bandwidth.length() == 0, "Missing parameter: bandwidth");
  NS_ABORT_MSG_IF(filesize.length() == 0, "Missing parameter: filesize");
  if (dw0.length() == 0) {
      dw0 = "0.5";
  }
  if (dw1.length() == 0) {
      dw1 = "0.5";
  }
  if (dw2.length() == 0) {
      dw2 = "0.5";
  }
  if (bw0.length() == 0) {
      bw0 = "0.5";
  }
  if (bw1.length() == 0) {
      bw0 = "0.5";
  }
  if (bw2.length() == 0) {
      bw0 = "0.5";
  }

  bw_vals[0] = atof(bw0.c_str());
  bw_vals[1] = atof(bw1.c_str());
  bw_vals[2] = atof(bw2.c_str());
  double bw_sum = bw_vals[0] + bw_vals[1] + bw_vals[2];

  dw_vals[0] = atof(dw0.c_str());
  dw_vals[1] = atof(dw1.c_str());
  dw_vals[2] = atof(dw2.c_str());
  double dw_sum = dw_vals[0] + dw_vals[1] + dw_vals[2];

  double sum_bandwidth = atof(bandwidth.c_str());
  double sum_delay = atof(delay.c_str());
  int queue_val = atoi(queue.c_str());

  SetDrops(client_drops[0], c0d);
  SetDrops(client_drops[1], c1d);
  SetDrops(client_drops[2], c2d);
  SetDrops(server_drops[0], s0d);
  SetDrops(server_drops[1], s1d);
  SetDrops(server_drops[2], s2d);

  std::vector<std::string> filesizes;
  filesizes.push_back(filesize);
  QuicNetworkSimulatorHelper sim = QuicNetworkSimulatorHelper(filesizes);

  for (int i = 0; i < 3; i++) {
      double b = (sum_bandwidth * bw_vals[i] / bw_sum);
      double d = (sum_delay * dw_vals[i] / dw_sum);
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

      devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(client_drops[i]));
      devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(server_drops[i]));
  }

  sim.Run(Seconds(180));
}
