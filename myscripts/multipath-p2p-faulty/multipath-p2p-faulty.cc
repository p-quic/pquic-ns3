#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "../helper/quic-network-simulator-helper.h"
#include "../helper/quic-point-to-point-helper.h"
#include "../helper/blackhole-error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ns3 simulator");

int main(int argc, char *argv[]) {
    std::string delay, delay_bal, queue, bandwidth, bandwidth_bal, filesize, on, off, repeat_s, drop_direction_s;
    CommandLine cmd;

    cmd.AddValue("delay", "average delay of the p2p links", delay);
    cmd.AddValue("bandwidth", "sum of bandwidth of the p2p links", bandwidth);
    cmd.AddValue("queue", "unused", queue);
    cmd.AddValue("delay_balance", "percentage of delay balance", delay_bal);
    cmd.AddValue("bandwidth_balance", "percentage of bandwidth balance", bandwidth_bal);
    cmd.AddValue("filesize", "filesize to request (in bytes)", filesize);

    cmd.AddValue("on", "time that the second link is active (e.g. 15s)", on);
    cmd.AddValue("off", "time that the second link is dropping all packets (e.g. 2s)", off);
    cmd.AddValue("repeat", "(optional) turn the second link on and off this many times. Default: 1", repeat_s);
    cmd.AddValue("direction", "(optional) [ both, toclient, toserver ] direction in which to drop packet. Default: both",
                 drop_direction_s);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(delay.length() == 0, "Missing parameter: delay");
    NS_ABORT_MSG_IF(bandwidth.length() == 0, "Missing parameter: bandwidth");
    NS_ABORT_MSG_IF(filesize.length() == 0, "Missing parameter: filesize");
    NS_ABORT_MSG_IF(on.length() == 0, "Missing parameter: on");
    NS_ABORT_MSG_IF(off.length() == 0, "Missing parameter: off");
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

    int repeat = 1;
    if (repeat_s.length() > 0) {
        repeat = std::stoi(repeat_s);
    }
    NS_ABORT_MSG_IF(repeat <= 0, "Invalid value: repeat value must be greater than zero.");

    enum drop_direction {
        both, to_client, to_server
    };

    drop_direction drop_dir = both;
    if (drop_direction_s.length() > 0) {
        if (drop_direction_s == "toclient") drop_dir = to_client;
        else if (drop_direction_s == "toserver") drop_dir = to_server;
        else if (drop_direction_s == "both") drop_dir = both;
        else NS_ABORT_MSG("Invalid directon value.");
    }

    QuicNetworkSimulatorHelper sim = QuicNetworkSimulatorHelper(filesize);

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

        if (i == 1) {
            Ptr <BlackholeErrorModel> em = CreateObject<BlackholeErrorModel>();
            em->Disable();
            if (drop_dir == to_client || drop_dir == both) {
                devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
            }
            if (drop_dir == to_server || drop_dir == both) {
                devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
            }

            Time intv = Time(on) + Time(off);
            Simulator::Schedule(Time(on), &Enable, em, intv, repeat);
            Simulator::Schedule(intv, &Disable, em, intv, repeat);
        }
    }

    sim.Run(Seconds(180));
}
