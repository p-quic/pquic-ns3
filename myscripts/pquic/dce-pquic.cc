#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "misc-tools.h"

#ifdef DCE_MPI
#include "ns3/mpi-interface.h"
#endif

using namespace ns3;

// ===========================================================================
//
//         node 0                 node 1
//   +----------------+    +----------------+
//   |                |    |                |
//   +----------------+    +----------------+
//   |    10.1.1.1    |    |    10.1.1.2    |
//   +----------------+    +----------------+
//   | point-to-point |    | point-to-point |
//   +----------------+    +----------------+
//           |                     |
//           +---------------------+
//                10 Mbps, 5 ms
//
//
// wget http://www.skbuff.net/iputils/iputils-s20101006.tar.bz2
//
//recompile iputils: edit Makefile: replace "CFLAGS=" with "CFLAGS+=" and run:
//                   "make CFLAGS=-fPIC LDFLAGS=-pie ping"
// ===========================================================================
int main(int argc, char *argv[]) {
    uint32_t systemId = 0;
    uint32_t systemCount = 1;
#ifdef DCE_MPI
    // Distributed simulation setup
    MpiInterface::Enable (&argc, &argv);
    GlobalValue::Bind ("SimulatorImplementationType",
                       StringValue ("ns3::DistributedSimulatorImpl"));
    systemId = MpiInterface::GetSystemId ();
    systemCount = MpiInterface::GetSize ();
#endif

    bool useKernel = 0;
    CommandLine cmd;
    cmd.AddValue("kernel", "Use kernel linux IP stack.", useKernel);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    Ptr <Node> node0 = CreateObject<Node>(0 % systemCount); // Create node0 with system id 0
    Ptr <Node> node1 = CreateObject<Node>(1 % systemCount); // Create node1 with system id 1
    nodes.Add(node0);
    nodes.Add(node1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    if (!useKernel) {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.252");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // setup ip routes
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    DceManagerHelper dceManager;

    if (useKernel) {
        dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library", StringValue("liblinux.so"));

        AddAddress(nodes.Get(0), Seconds(0.1), "sim0", "10.1.1.1/8");
        RunIp(nodes.Get(0), Seconds(0.2), "link set sim0 up arp off");

        AddAddress(nodes.Get(1), Seconds(0.3), "sim0", "10.1.1.2/8");
        RunIp(nodes.Get(1), Seconds(0.4), "link set sim0 up arp off");

    }
    dceManager.Install(nodes);

    DceApplicationHelper dce;
    ApplicationContainer apps;

    dce.SetStackSize(1 << 20);

    if (systemId == node0->GetSystemId()) {
        // Launch picoquicdemo in server mode on node 0
        dce.SetBinary("picoquicdemomp");
        dce.ResetArguments();
        dce.ResetEnvironment();

        apps = dce.Install(node0);
        apps.Start(Seconds(1.0));
    }
    if (systemId == node1->GetSystemId()) {
        // Launch picoquicdemo in client mode on node 1
        dce.SetBinary("picoquicdemomp");
        dce.ResetArguments();
        dce.ResetEnvironment();
        dce.AddArgument("10.1.1.1");
        dce.AddArgument("4443");

        apps = dce.Install(node1);
        apps.Start(Seconds(2.0));
    }

    // Simulator::Schedule(Seconds(3), &pointToPoint.SetDeviceAttribute, "DataRate", StringValue("0Mbps"))

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

#ifdef DCE_MPI
    MpiInterface::Disable ();
#endif

    return 0;
}


