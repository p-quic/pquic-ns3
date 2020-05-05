#include "ns3/core-module.h"
#include "ns3/traffic-control-helper.h"
#include "quic-point-to-point-helper.h"

using namespace ns3;

QuicPointToPointHelper::QuicPointToPointHelper() : queue_size_(StringValue("100p")) {
  SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));
}

void QuicPointToPointHelper::SetQueueSize(StringValue size) {
  queue_size_ = size;
}

void QuicPointToPointHelper::SetBandwidth(StringValue bw) {
    bandwidth_ = bw;
}

void QuicPointToPointHelper::SetDelay(StringValue de) {
    delay_ = de;
}

NetDeviceContainer QuicPointToPointHelper::Install(Ptr<Node> a, Ptr<Node> b, std::string queue_class) {
  NetDeviceContainer devices = PointToPointHelper::Install(a, b);
  // capture a pcap of all packets
  //EnablePcap("trace_node_left.pcap", devices.Get(0), false, true);
  //EnablePcap("trace_node_right.pcap", devices.Get(1), false, true);
  
  TrafficControlHelper tch;
  if (queue_class == "ns3::RedQueueDisc") {
      tch.SetRootQueueDisc(queue_class, "MaxSize", queue_size_, "LinkBandwidth", bandwidth_, "LinkDelay", delay_);
  } else {
      tch.SetRootQueueDisc(queue_class, "MaxSize", queue_size_);
  }
  tch.Install(devices);

  return devices;
}
