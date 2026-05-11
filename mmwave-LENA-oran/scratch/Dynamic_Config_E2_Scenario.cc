/* Dynamic Config E2 Scenario
 * Purpose: Single gNB + multiple UEs, controlled by external file 'e2_command.txt'
 * Acts as a Mock E2 Interface.
 */

#include "ns3/mmwave-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-enb-net-device.h"
#include "ns3/node-container-manager.h"
#include <fstream>
#include <iostream>
#include <cstdio>

using namespace ns3;
using namespace mmwave;

NS_LOG_COMPONENT_DEFINE("DynamicConfigE2Scenario");

// Constants to match RicControlMessage enums (manually synced)
const long STYLE_MOBILITY = 3; 
const long ACTION_HANDOVER = 1;
const long STYLE_ENERGY = 4;

void CheckCommandFile (Ptr<MmWaveEnbNetDevice> dev)
{
  std::string filename = "e2_command.txt";
  std::ifstream file(filename);
  
  // Debug print every 1s (approx) to show we are alive
  static int counter = 0;
  if (counter++ % 10 == 0) {
      // NS_LOG_UNCOND("Mock E2: Checking for " << filename << " (Alive)");
  }

  if (file.good()) {
     long style, action;
     int target;
     uint64_t imsi; // or power value
     
     if (file >> style >> action >> target >> imsi) {
         std::cout << "Mock E2: Executing Command - Style " << style << " Action " << action << " Target " << target << " Val " << imsi << std::endl;
         dev->ExecRicControl(style, action, target, imsi);
     } else {
         std::cout << "Mock E2: File found but failed to parse content." << std::endl;
     }
     
     file.close();
     if (std::remove(filename.c_str()) != 0) {
         std::cout << "Mock E2: Failed to delete file " << filename << std::endl;
     }
  }
  
  Simulator::Schedule(MilliSeconds(100), &CheckCommandFile, dev);
}

int main(int argc, char *argv[])
{
  // Configuration
  uint32_t numUes = 20;
  double simTime = 10.0;  // seconds
  bool enableTraces = true;
  
  CommandLine cmd;
  cmd.AddValue("numUes", "Number of UEs", numUes);
  cmd.AddValue("simTime", "Simulation time", simTime);
  cmd.Parse(argc, argv);
  
  std::cout << "Dynamic Config E2 Scenario Started. Waiting for CheckCommandFile..." << std::endl;
  std::cout << "Simulation configured for " << simTime << " seconds." << std::endl;

  // 1. Create Nodes
  NodeContainer gnbNode;
  NodeContainer ueNodes;
  gnbNode.Create(1);
  ueNodes.Create(numUes);

  // 2. Install Mobility
  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gnbMobility.Install(gnbNode);
  gnbNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 10.0));

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  ueMobility.Install(ueNodes);
  
  // Place UEs: First 5 Active (close), Rest Inactive (far)
  // We will swap them using E2 (Handover logic or just by moving them? 
  // Handover requires another cell usually.
  // Wait, the user asked to "circulate UEs".
  // If we only have 1 cell, we can't "Handover" to another cell in the standard sense unless we have neighbor cells.
  // The Prompt: "circulate UEs so their count would be dynamic".
  // Approach: 
  // 1. Start with 1 gNB (ID 1).
  // 2. Ideally we need a 'dummy' neighbor cell (ID 2) to handover 'out' to?
  //    Or we just use Power Control to 'mute' the cell? No that affects all UEs.
  //    Or we move UEs far away? 
  // E2 Handover Logic in MmWaveEnbNetDevice expects a target cell ID.
  // If we only have 1 gNB, Handover functionality is limited unless we create a virtual neighbor.
  // 
  // ALTERNATIVE: Create 2 gNBs. 
  // gNB 1 is the "Active" cell we care about.
  // gNB 2 is a "Holding" cell far away.
  // We circulate UEs by handing them over between Cell 1 and Cell 2.
  
  NodeContainer gnbNodes;
  gnbNodes.Create(2); // ID 0 and 1
  
  // Set positions
  Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator> ();
  gnbPos->Add (Vector (0.0, 0.0, 10.0)); // Cell 1 (Active)
  gnbPos->Add (Vector (100.0, 0.0, 10.0)); // Cell 2 (Closer)
  
  gnbMobility.SetPositionAllocator(gnbPos);
  gnbMobility.Install(gnbNodes);
  
  // Register gNBs to NodeContainerManager for ExecRicControl
  NodeContainerManager::GetInstance().SetMmWaveEnbNodes(gnbNodes);

  // Initial Placement of UEs
  // All UEs attached to Cell 1 initially? Or split?
  // Let's put 5 UEs close to Cell 1, 15 UEs close to Cell 2.
  for (uint32_t i = 0; i < numUes; i++)
  {
      double x, y;
      if (i < 5) { // Active Group 1
          x = 10.0 * cos(2*M_PI*i/5.0);
          y = 10.0 * sin(2*M_PI*i/5.0);
      } else { // Inactive Group (at Cell 2)
          x = 10000.0 + 10.0;
          y = 10000.0;
      }
      ueNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(x, y, 1.5));
  }

  // 3. Configure MmWave
  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
  mmwaveHelper->SetChannelModelType("");  
  mmwaveHelper->SetPathlossModelType("ns3::FriisPropagationLossModel");
  mmwaveHelper->SetBeamformingModelType("ns3::MmWaveDftBeamforming");

  Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(28e9));
  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(400e6));
  
  // PHY Parameters
  Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(30.0));

  // 4. Install EPC
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
  mmwaveHelper->SetEpcHelper(epcHelper);

  // 5. Install Devices
  NetDeviceContainer gnbDevs = mmwaveHelper->InstallEnbDevice(gnbNodes);
  
  // Create X2 Interface between all gNBs
  mmwaveHelper->AddX2Interface(gnbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

  // 6. Internet Stack
  InternetStackHelper internet;
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
  
  for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
     Ptr<Node> ueNode = ueNodes.Get(u);
     Ptr<Ipv4StaticRouting> ueStaticRouting = 
       Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
         ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
     ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
  }

  // 7. Attach UEs
  // Attach first 5 to Cell 1 (Index 0), rest to Cell 2 (Index 1)
  // mmwaveHelper->AttachToClosestEnb(ueDevs, gnbDevs);
  // Manual attachment to ensure initial state
  for (uint32_t i=0; i<numUes; i++) {
      if (i < 5) {
          mmwaveHelper->AttachToEnbWithIndex(ueDevs.Get(i), gnbDevs, 0);
      } else {
          mmwaveHelper->AttachToEnbWithIndex(ueDevs.Get(i), gnbDevs, 1);
      }
  }

  // 8. Applications (Traffic)
  uint16_t dlPort = 1234;
  ApplicationContainer serverApps, clientApps;
  
  for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
  {
    UdpServerHelper dlPacketSinkHelper(dlPort);
    serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));
    
    UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(dlClient.Install(epcHelper->GetPgwNode()));
  }
  
  serverApps.Start(Seconds(0.1));
  clientApps.Start(Seconds(0.1));

  if (enableTraces) {
    mmwaveHelper->EnableTraces();
  }
  
  // 9. Setup Mock E2 Listener on gNB 1 (The Active Cell)
  // We want to control gNB 1 (change power, handover UEs TO/FROM it)
  // gNB 1 is gnbNodes.Get(0), device 0
  Ptr<MmWaveEnbNetDevice> gnbDev1 = DynamicCast<MmWaveEnbNetDevice>(gnbDevs.Get(0));
  Simulator::Schedule(Seconds(0.1), &CheckCommandFile, gnbDev1);

  // 10. Run
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
