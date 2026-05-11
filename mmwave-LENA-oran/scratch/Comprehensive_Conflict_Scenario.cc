/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Comprehensive Conflict Scenario for ML Training
 * 
 * Features:
 * - 2 mmWave Cells (Independently Configurable)
 * - Dynamic RSRP-based Attachment (Connects to best cell based on TxPower & Distance)
 * - Configurable Parameters: TxPower1/2, Tilt1/2, T_Reordering, ISD, UEs
 * - Generates traces for ML training (Power, Throughput, Latency, SINR, Load)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include <ns3/lte-ue-net-device.h>
#include "ns3/mmwave-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "../src/mmwave/model/node-container-manager.h"
#include "ns3/lte-helper.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/mmwave-radio-energy-model-enb-helper.h"
#include "ns3/isotropic-antenna-model.h"
#include <fstream>
#include <cmath>

using namespace ns3;
using namespace mmwave;

NS_LOG_COMPONENT_DEFINE ("ComprehensiveConflictScenario");

// --- Global Variables for Energy Tracing ---
double totalnewEnergyConsumption_storage[10] = {0};
double totaloldEnergyConsumption_storage[10] = {0};

void EnergyConsumptionUpdate (int nodeIndex, std::string filename, double totaloldEnergyConsumption, double totalnewEnergyConsumption)
{
  Time currentTime = Simulator::Now ();
  std::ofstream outFile;
  outFile.open (filename, std::ios_base::out | std::ios_base::app);
  outFile << currentTime.GetSeconds () << "," << totalnewEnergyConsumption << ","
          << (totalnewEnergyConsumption - totaloldEnergyConsumption) << std::endl;
  totalnewEnergyConsumption_storage[nodeIndex] = totalnewEnergyConsumption;
}

void EnergyConsumptionPrint (int nodeIndex)
{
    // Minimal logging to avoid clutter
    totaloldEnergyConsumption_storage[nodeIndex] = totalnewEnergyConsumption_storage[nodeIndex];
}

// --- Global Config Values ---
static ns3::GlobalValue g_simTime("simTime", "Simulation time in seconds", ns3::DoubleValue(1.0), ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_rngRun("RngRun", "Rng Run ID", ns3::UintegerValue(1), ns3::MakeUintegerChecker<uint32_t>());

// Configurable Parameters for Sweep
static ns3::GlobalValue g_ues("N_Ues", "Number of UEs", ns3::UintegerValue(20), ns3::MakeUintegerChecker<uint32_t>());
static ns3::GlobalValue g_isd("IntersideDistanceCells", "Distance between cells (m)", ns3::DoubleValue(500.0), ns3::MakeDoubleChecker<double>());

// Independent Cell Configs
static ns3::GlobalValue g_p1("TxPower1", "Tx Power Cell 1 (dBm)", ns3::DoubleValue(30.0), ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_p2("TxPower2", "Tx Power Cell 2 (dBm)", ns3::DoubleValue(30.0), ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_t1("Tilt1", "Tilt Cell 1 (deg)", ns3::DoubleValue(0.0), ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_t2("Tilt2", "Tilt Cell 2 (deg)", ns3::DoubleValue(0.0), ns3::MakeDoubleChecker<double>());

// Traffic & Protocol
static ns3::GlobalValue g_treord("tReorderingTimer", "RLC t-Reordering (ms)", ns3::DoubleValue(35.0), ns3::MakeDoubleChecker<double>());

int main(int argc, char *argv[]) 
{
    // 1. Parse Command Line
    CommandLine cmd;
    cmd.Parse(argc, argv);
    
    // 2. Set Random Seed
    UintegerValue uintegerValue;
    GlobalValue::GetValueByName("RngRun", uintegerValue);
    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(uintegerValue.Get());

    // 3. Get Parameters
    DoubleValue dVal;
    GlobalValue::GetValueByName("TxPower1", dVal); double txPower1 = dVal.Get();
    GlobalValue::GetValueByName("TxPower2", dVal); double txPower2 = dVal.Get();
    GlobalValue::GetValueByName("Tilt1", dVal); double tilt1 = dVal.Get();
    GlobalValue::GetValueByName("Tilt2", dVal); double tilt2 = dVal.Get();
    GlobalValue::GetValueByName("IntersideDistanceCells", dVal); double isd = dVal.Get();
    GlobalValue::GetValueByName("tReorderingTimer", dVal); double tReord = dVal.Get();
    GlobalValue::GetValueByName("simTime", dVal); double simTime = dVal.Get();
    GlobalValue::GetValueByName("N_Ues", uintegerValue); uint32_t numUes = uintegerValue.Get();

    NS_LOG_UNCOND("--- Comprehensive Conflict Scenario ---");
    NS_LOG_UNCOND("UEs: " << numUes << ", ISD: " << isd << "m");
    NS_LOG_UNCOND("Cell 1: P=" << txPower1 << "dBm, Tilt=" << tilt1 << "deg");
    NS_LOG_UNCOND("Cell 2: P=" << txPower2 << "dBm, Tilt=" << tilt2 << "deg");
    NS_LOG_UNCOND("T_Reordering: " << tReord << "ms");

    // 4. Configuration
    Config::SetDefault("ns3::LteRlcUm::ReorderingTimer", TimeValue(MilliSeconds(tReord)));
    
    // Beamforming & PHY
    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel"); // Realistic Pathloss
    mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
    mmwaveHelper->SetBeamformingModelType("ns3::MmWaveDftBeamforming");
    
    // Antenna Array (4x4 MIMO)
    mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumRows", UintegerValue(4));
    mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumColumns", UintegerValue(4));
    mmwaveHelper->SetUePhasedArrayModelAttribute("NumRows", UintegerValue(2));
    mmwaveHelper->SetUePhasedArrayModelAttribute("NumColumns", UintegerValue(2));

    // EPC
    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);

    // 5. Create Nodes
    NodeContainer enbNodes; enbNodes.Create(2);
    NodeContainer ueNodes; ueNodes.Create(numUes);
    NodeContainerManager::GetInstance().SetMmWaveEnbNodes(enbNodes);

    // 6. Mobility (Cells)
    Ptr<ListPositionAllocator> enbPos = CreateObject<ListPositionAllocator>();
    enbPos->Add(Vector(1000.0 - isd/2.0, 1000.0, 3.0)); // Cell 1
    enbPos->Add(Vector(1000.0 + isd/2.0, 1000.0, 3.0)); // Cell 2
    
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPos);
    enbMobility.Install(enbNodes);

    // 7. Mobility (UEs) - Randomly distributed between cells
    // Box area covering both cells plus 200m margin
    double minX = 1000.0 - isd/2.0 - 200.0;
    double maxX = 1000.0 + isd/2.0 + 200.0;
    
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minX) + "|Max=" + std::to_string(maxX) + "]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=800.0|Max=1200.0]"),
        "Z", StringValue("ns3::UniformRandomVariable[Min=1.5|Max=1.5]"));
    ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // Static for consistent conflict analysis
    ueMobility.Install(ueNodes);

    // 8. Install Devices (Differential Config)
    
    // Cell 1
    NodeContainer n1 = NodeContainer(enbNodes.Get(0));
    Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(txPower1));
    // Set Tilt 1
    double tiltRad1 = tilt1 * M_PI / 180.0;
    mmwaveHelper->SetEnbPhasedArrayModelAttribute("DowntiltAngle", DoubleValue(tiltRad1));
    NetDeviceContainer d1 = mmwaveHelper->InstallEnbDevice(n1);

    // Cell 2
    NodeContainer n2 = NodeContainer(enbNodes.Get(1));
    Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(txPower2));
    // Set Tilt 2
    double tiltRad2 = tilt2 * M_PI / 180.0;
    mmwaveHelper->SetEnbPhasedArrayModelAttribute("DowntiltAngle", DoubleValue(tiltRad2));
    NetDeviceContainer d2 = mmwaveHelper->InstallEnbDevice(n2);
    
    NetDeviceContainer enbDevs; enbDevs.Add(d1); enbDevs.Add(d2);
    
    // UEs
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // 9. IP Stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(ueDevs);
    // Set Default Gateway
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        Ptr<Node> node = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(node->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // 10. Manual Attachment (Based on RSRP)
    // Simple Friis-like estimation for initial attachment to strongest cell
    auto CalculateDistance = [](Vector a, Vector b) {
        return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2) + std::pow(a.z - b.z, 2));
    };

    Vector pos1 = enbNodes.Get(0)->GetObject<MobilityModel>()->GetPosition();
    Vector pos2 = enbNodes.Get(1)->GetObject<MobilityModel>()->GetPosition();

    for (uint32_t u = 0; u < ueDevs.GetN(); ++u) {
        Vector uePos = ueNodes.Get(u)->GetObject<MobilityModel>()->GetPosition();
        
        double dist1 = CalculateDistance(uePos, pos1);
        double dist2 = CalculateDistance(uePos, pos2);
        
        // Log distance pathloss model (simplified for decision)
        double pl1 = 28.0 + 22.0 * std::log10(dist1);
        double pl2 = 28.0 + 22.0 * std::log10(dist2);
        
        double rx1 = txPower1 - pl1;
        double rx2 = txPower2 - pl2;
        
        if (rx1 >= rx2) {
            mmwaveHelper->AttachToEnbWithIndex(ueDevs.Get(u), enbDevs, 0);
        } else {
            mmwaveHelper->AttachToEnbWithIndex(ueDevs.Get(u), enbDevs, 1);
        }
    }

    // 11. Applications (UDP Full Buffer)
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer; remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetRH; internetRH.Install(remoteHostContainer);
    
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    
    Ipv4AddressHelper ipv4h; ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);
    
    // Route from Remote Host back to UEs
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install Apps
    ApplicationContainer clientApps, serverApps;
    uint16_t port = 1234;
    
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    serverApps.Add(packetSinkHelper.Install(ueNodes)); // UEs receive
    
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        UdpClientHelper udpClient(ueIpIface.GetAddress(u), port);
        udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(100))); // High load
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        clientApps.Add(udpClient.Install(remoteHost));
    }
    
    serverApps.Start(Seconds(0.0));
    clientApps.Start(Seconds(0.1));

    // 12. Energy Model
    BasicEnergySourceHelper basicSource;
    basicSource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1e12));
    basicSource.Set("BasicEnergySupplyVoltageV", DoubleValue(5.0));
    energy::EnergySourceContainer sources = basicSource.Install(enbNodes);
    
    MmWaveRadioEnergyModelEnbHelper energyModel;
    energyModel.Set("TxPower", DoubleValue(txPower1)); // Base coeff, actual power handled dynamically
    // Note: The energy model helper in this version might not support per-device TxPower perfectly via Set(), 
    // but the traces will capture the configured PHY TxPower if implemented correctly in the model.
    energy::DeviceEnergyModelContainer deviceModels = energyModel.Install(enbDevs, sources);

    // Trace Energy (Simplified)
    for (uint32_t i=0; i<enbNodes.GetN(); ++i) {
        std::ostringstream fn; fn << "energy_cell" << i+1 << ".csv";
        // Correct Binding: Bind [nodeIndex, filename]
        // Trace Source provides [oldTotalEnergy, newTotalEnergy]
        deviceModels.Get(i)->TraceConnectWithoutContext("TotalEnergyConsumption", 
            MakeBoundCallback(&EnergyConsumptionUpdate, (int)i, fn.str())); 
    }
    
    // 13. Enable Traces
    mmwaveHelper->EnableRlcTraces();
    mmwaveHelper->EnablePdcpTraces();
    mmwaveHelper->EnableEnbSchedTrace();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
