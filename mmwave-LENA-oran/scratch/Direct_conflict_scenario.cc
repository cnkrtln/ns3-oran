/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/* *
 * Copyright (c) 2024 Orange Innovation Poland
 * Copyright (c) 2024 Orange Innovation Egypt
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <sys/time.h>
#include <ctime>
#include <sys/types.h>
#include <iostream>
#include <stdlib.h>
#include <list>
#include <random>
#include <chrono>
#include <cmath>
#include <fstream>
#include "ns3/basic-energy-source-helper.h"
#include "ns3/mmwave-radio-energy-model-enb-helper.h"
#include "ns3/isotropic-antenna-model.h"

using namespace ns3;
using namespace mmwave;

std::map<uint64_t, uint16_t> imsi_cellid;
std::map<uint16_t, std::set<uint64_t>> imsi_list;
std::map<uint16_t, Ptr < Node>> cellid_node;
std::map<uint32_t, uint16_t> ue_cellid_usinghandover;
std::map<uint64_t, uint32_t> ueimsi_nodeid;
std::map<uint64_t, int> ue_assoc_list;
double maxXAxis;
double maxYAxis;
bool esON_list[10] = {0};
double totalnewEnergyConsumption_storage[10] = {0};
double totaloldEnergyConsumption_storage[10] = {0};
double current_energy_consumption[10] = {0};
double curr_total_energy_consumption = 0;
double max_energy_consumption = 0;
double sum_curr_total_energy_consumption = 0;
int num_of_mmdev = 0;

NS_LOG_COMPONENT_DEFINE ("DirectConflictScenario");

void
EnergyConsumptionUpdate (int nodeIndex, std::string filename, double totaloldEnergyConsumption,
                         double totalnewEnergyConsumption)
{
  Time currentTime = Simulator::Now ();
  std::ofstream outFile;
  outFile.open (filename, std::ios_base::out | std::ios_base::app);
  outFile << currentTime.GetSeconds () << "," << totalnewEnergyConsumption << ","
          << (totalnewEnergyConsumption - totaloldEnergyConsumption) << std::endl;
  totalnewEnergyConsumption_storage[nodeIndex] = totalnewEnergyConsumption;
}

void
EnergyConsumptionPrint (int nodeIndex)
{
  NS_LOG_UNCOND ("Total energy consumption for mmWave cell "
                 << nodeIndex + 2 << ": " << totalnewEnergyConsumption_storage[nodeIndex] << "J"
                 << " at time " << Simulator::Now ().GetSeconds ()
                 << ", diff from last measurement is: "
                 << (totalnewEnergyConsumption_storage[nodeIndex] -
                     totaloldEnergyConsumption_storage[nodeIndex])
                 << "J");
  totalnewEnergyConsumption_storage[nodeIndex] = totalnewEnergyConsumption_storage[nodeIndex];
  current_energy_consumption[nodeIndex] =
      totalnewEnergyConsumption_storage[nodeIndex] - totaloldEnergyConsumption_storage[nodeIndex];
  totaloldEnergyConsumption_storage[nodeIndex] = totalnewEnergyConsumption_storage[nodeIndex];
}

// Global Values
static ns3::GlobalValue g_bufferSize("bufferSize", "RLC tx buffer size (MB)",
                                      ns3::UintegerValue(10),
                                      ns3::MakeUintegerChecker<uint32_t>());

static ns3::GlobalValue g_enableTraces("enableTraces", "If true, generate ns-3 traces",
                                        ns3::BooleanValue(true), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2lteEnabled("e2lteEnabled", "If true, send LTE E2 reports",
                                        ns3::BooleanValue(false), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2nrEnabled("e2nrEnabled", "If true, send NR E2 reports",
                                       ns3::BooleanValue(true), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2du("e2du", "If true, send DU reports", ns3::BooleanValue(true),
                                ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2cuUp("e2cuUp", "If true, send CU-UP reports", ns3::BooleanValue(true),
                                  ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2cuCp("e2cuCp", "If true, send CU-CP reports", ns3::BooleanValue(true),
                                  ns3::MakeBooleanChecker());

static ns3::GlobalValue g_reducedPmValues("reducedPmValues", "If true, use a subset of the the pm containers",
                                          ns3::BooleanValue(false), ns3::MakeBooleanChecker());

static ns3::GlobalValue
    g_hoSinrDifference("hoSinrDifference",
                        "The value for which an handover between MmWave eNB is triggered",
                        ns3::DoubleValue(3), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue
    g_indicationPeriodicity("indicationPeriodicity",
                             "E2 Indication Periodicity reports (value in seconds)",
                             ns3::DoubleValue(0.1), ns3::MakeDoubleChecker<double>(0.01, 2.0));

static ns3::GlobalValue g_simTime("simTime", "Simulation time in seconds", ns3::DoubleValue(1000),
                                   ns3::MakeDoubleChecker<double>(0.1, 100000.0));

static ns3::GlobalValue g_outageThreshold("outageThreshold",
                                           "SNR threshold for outage events [dB]", // use -1000.0 with NoAuto
                                           ns3::DoubleValue(-5.0),
                                           ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_numberOfRaPreambles(
    "numberOfRaPreambles",
    "how many random access preambles are available for the contention based RACH process",
    ns3::UintegerValue(40), // Indicated for TS use case, 52 is default
    ns3::MakeUintegerChecker<uint8_t>());

static ns3::GlobalValue
    g_handoverMode("handoverMode",
                    "HO euristic to be used,"
                    "can be only \"NoAuto\", \"FixedTtt\", \"DynamicTtt\",   \"Threshold\"",
                    ns3::StringValue("DynamicTtt"), ns3::MakeStringChecker());

static ns3::GlobalValue g_e2TermIp("e2TermIp", "The IP address of the RIC E2 termination",
                                    ns3::StringValue("127.0.0.1"), ns3::MakeStringChecker());

static ns3::GlobalValue
        g_enableE2FileLogging("enableE2FileLogging",
                              "If true, generate offline file logging instead of connecting to RIC",
                              ns3::BooleanValue(false), ns3::MakeBooleanChecker());
static ns3::GlobalValue g_e2_func_id("KPM_E2functionID", "Function ID to subscribe",
                                      ns3::DoubleValue(2),
                                      ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_rc_e2_func_id("RC_E2functionID", "Function ID to subscribe",
                                         ns3::DoubleValue(3),
                                         ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_e2andLogging("e2andLogging", "If true, both RIC connection and file logging",
                                      ns3::BooleanValue(false), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_controlFileName("controlFileName",
                                           "The path to the control file (can be absolute)",
                                           ns3::StringValue(""),
                                           ns3::MakeStringChecker());

static ns3::GlobalValue mmWave_nodes ("N_MmWaveEnbNodes", "Number of mmWaveNodes",
                                      ns3::UintegerValue (2),
                                      ns3::MakeUintegerChecker<uint8_t> ());

static ns3::GlobalValue lteEnb_nodes ("N_LteEnbNodes", "Number of LteEnbNodes",
                                      ns3::UintegerValue (0),
                                      ns3::MakeUintegerChecker<uint8_t> ());

static ns3::GlobalValue ue_s ("N_Ues", "Number of User Equipments",
                              ns3::UintegerValue (20),
                              ns3::MakeUintegerChecker<uint32_t> ());

static ns3::GlobalValue center_freq ("CenterFrequency", "Center Frequency Value",
                                     ns3::DoubleValue (3.5e9),
                                     ns3::MakeDoubleChecker<double> ());

static ns3::GlobalValue bandwidth_value ("Bandwidth", "Bandwidth Value",
                                         ns3::DoubleValue (20e6),
                                         ns3::MakeDoubleChecker<double> ());

static ns3::GlobalValue num_antennas_McUe ("N_AntennasMcUe", "Number of Antenna as McUe",
                                      ns3::UintegerValue (1),
                                      ns3::MakeUintegerChecker<uint32_t> ());

static ns3::GlobalValue num_antennas_MmWave ("N_AntennasMmWave", "Number of Antenna as MmWave",
                                      ns3::UintegerValue (1),
                                      ns3::MakeUintegerChecker<uint32_t> ());

static ns3::GlobalValue interside_distance_value_ue ("IntersideDistanceUEs", "Interside Distance Value",
                                      ns3::DoubleValue (100),
                                      ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue interside_distance_value_cell ("IntersideDistanceCells", "Interside Distance Value",
                                                  ns3::DoubleValue (500),
                                                  ns3::MakeDoubleChecker<double> ());

// New GlobalValues for Conflict Scenario
static ns3::GlobalValue g_txPower("TxPower", "Transmission Power in dBm",
                                  ns3::DoubleValue(30.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_tilt("Tilt", "Antenna Downtilt in degrees",
                               ns3::DoubleValue(0.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_useFriis("useFriis", "Use Friis Propagation Model (Faster) instead of 3GPP",
                                   ns3::BooleanValue(false), ns3::MakeBooleanChecker());

int
main(int argc, char *argv[]) {
  LogComponentEnableAll(LOG_PREFIX_ALL);

  maxXAxis = 2000;
  maxYAxis = 2000;

  CommandLine cmd;
  cmd.Parse(argc, argv);

  bool harqEnabled = true;

  UintegerValue uintegerValue;
  BooleanValue booleanValue;
  StringValue stringValue;
  DoubleValue doubleValue;

  GlobalValue::GetValueByName("hoSinrDifference", doubleValue);
  double hoSinrDifference = doubleValue.Get();
  GlobalValue::GetValueByName("bufferSize", uintegerValue);
  uint32_t bufferSize = uintegerValue.Get();
  GlobalValue::GetValueByName("enableTraces", booleanValue);
  bool enableTraces = booleanValue.Get();
  GlobalValue::GetValueByName("outageThreshold", doubleValue);
  double outageThreshold = doubleValue.Get();
  GlobalValue::GetValueByName("handoverMode", stringValue);
  std::string handoverMode = stringValue.Get();
  GlobalValue::GetValueByName("e2TermIp", stringValue);
  std::string e2TermIp = stringValue.Get();
  GlobalValue::GetValueByName("enableE2FileLogging", booleanValue);
  bool enableE2FileLogging = booleanValue.Get();
  GlobalValue::GetValueByName("KPM_E2functionID", doubleValue);
  double g_e2_func_id = doubleValue.Get();
  GlobalValue::GetValueByName("RC_E2functionID", doubleValue);
  double g_rc_e2_func_id = doubleValue.Get();
  GlobalValue::GetValueByName("e2andLogging", booleanValue);
  bool e2andLogging = booleanValue.Get();
  GlobalValue::GetValueByName("useFriis", booleanValue);
  bool useFriis = booleanValue.Get();

  GlobalValue::GetValueByName("numberOfRaPreambles", uintegerValue);
  uint8_t numberOfRaPreambles = uintegerValue.Get();

  GlobalValue::GetValueByName("e2lteEnabled", booleanValue);
  bool e2lteEnabled = booleanValue.Get();
  GlobalValue::GetValueByName("e2nrEnabled", booleanValue);
  bool e2nrEnabled = booleanValue.Get();
  GlobalValue::GetValueByName("e2du", booleanValue);
  bool e2du = booleanValue.Get();
  GlobalValue::GetValueByName("e2cuUp", booleanValue);
  bool e2cuUp = booleanValue.Get();
  GlobalValue::GetValueByName("e2cuCp", booleanValue);
  bool e2cuCp = booleanValue.Get();

  GlobalValue::GetValueByName("reducedPmValues", booleanValue);
  bool reducedPmValues = booleanValue.Get();

  GlobalValue::GetValueByName("indicationPeriodicity", doubleValue);
  double indicationPeriodicity = doubleValue.Get();
  GlobalValue::GetValueByName("controlFileName", stringValue);
  std::string controlFilename = stringValue.Get();

  // Get TxPower and Tilt
  GlobalValue::GetValueByName("TxPower", doubleValue);
  double txPower = doubleValue.Get();
  GlobalValue::GetValueByName("Tilt", doubleValue);
  double tilt = doubleValue.Get();

  Config::SetDefault("ns3::LteEnbNetDevice::ControlFileName", StringValue(controlFilename));
  Config::SetDefault("ns3::LteEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
  Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity",
                      DoubleValue(indicationPeriodicity));

  Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));
  Config::SetDefault("ns3::MmWaveHelper::E2ModeNr", BooleanValue(e2nrEnabled));

  Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(e2du));
  Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
  Config::SetDefault("ns3::LteEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
  Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));
  Config::SetDefault("ns3::LteEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));

  Config::SetDefault("ns3::MmWaveEnbNetDevice::ReducedPmValues", BooleanValue(reducedPmValues));
  Config::SetDefault("ns3::LteEnbNetDevice::ReducedPmValues", BooleanValue(reducedPmValues));

  Config::SetDefault("ns3::LteEnbNetDevice::EnableE2FileLogging",
                      BooleanValue(enableE2FileLogging));
  Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableE2FileLogging",
                      BooleanValue(enableE2FileLogging));

  Config::SetDefault("ns3::LteEnbNetDevice::KPM_E2functionID",
                      DoubleValue(g_e2_func_id));
  Config::SetDefault("ns3::MmWaveEnbNetDevice::KPM_E2functionID",
                      DoubleValue(g_e2_func_id));

  Config::SetDefault("ns3::LteEnbNetDevice::RC_E2functionID",
                      DoubleValue(g_rc_e2_func_id));
  Config::SetDefault("ns3::MmWaveEnbNetDevice::RC_E2functionID",
                      DoubleValue(g_rc_e2_func_id));

  Config::SetDefault("ns3::LteEnbNetDevice::e2andLogging", BooleanValue(e2andLogging));
  Config::SetDefault("ns3::MmWaveEnbNetDevice::e2andLogging", BooleanValue(e2andLogging));

  Config::SetDefault("ns3::MmWaveEnbMac::NumberOfRaPreambles",
                      UintegerValue(numberOfRaPreambles));

  Config::SetDefault("ns3::MmWaveHelper::HarqEnabled", BooleanValue(harqEnabled));
  Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));
  Config::SetDefault("ns3::MmWaveHelper::E2TermIp", StringValue(e2TermIp));

  Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(harqEnabled));
  Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));

  Config::SetDefault("ns3::PhasedArrayModel::AntennaElement",
    PointerValue(CreateObject<IsotropicAntennaModel>()));
  Config::SetDefault ("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue (MilliSeconds (100.0)));
  Config::SetDefault ("ns3::ThreeGppChannelConditionModel::UpdatePeriod",
    TimeValue (MilliSeconds (100)));

  Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MilliSeconds(10.0)));
  Config::SetDefault("ns3::LteRlcUmLowLat::ReportBufferStatusTimer",
                      TimeValue(MilliSeconds(10.0)));
  Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
  Config::SetDefault("ns3::LteRlcUmLowLat::MaxTxBufferSize",
                      UintegerValue(bufferSize * 1024 * 1024));
  Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));

  Config::SetDefault("ns3::LteEnbRrc::OutageThreshold", DoubleValue(outageThreshold));
  Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue(handoverMode));
  Config::SetDefault("ns3::LteEnbRrc::HoSinrDifference", DoubleValue(hoSinrDifference));
  Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency",DoubleValue(3.5e9));
  Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled",BooleanValue(false));
  
  // Set TxPower
  Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(txPower));
  // Note: Tilt is typically handled by the AntennaModel or BeamformingModel. 
  // For IsotropicAntennaModel, tilt doesn't apply directly in the same way as phased arrays.
  // However, we can simulate it by adjusting the antenna orientation if we were using a directional antenna.
  // Since we use IsotropicAntennaModel as per requirement, 'Tilt' might be a placeholder or require a different antenna model.
  // But let's assume we might switch to a directional model later or it's for the PhasedArrayModel configuration.
  // For now, we will log it.
  NS_LOG_UNCOND("TxPower: " << txPower << " dBm, Tilt: " << tilt << " degrees");

  GlobalValue::GetValueByName ("Bandwidth", doubleValue);
  double bandwidth = doubleValue.Get ();
  GlobalValue::GetValueByName ("CenterFrequency", doubleValue);
  double centerFrequency = doubleValue.Get ();
  GlobalValue::GetValueByName ("IntersideDistanceUEs", doubleValue);
  double isd_ue = doubleValue.Get (); 
  GlobalValue::GetValueByName ("IntersideDistanceCells", doubleValue);
  double isd_cell = doubleValue.Get (); 

  GlobalValue::GetValueByName ("N_AntennasMcUe", uintegerValue);
  int numAntennasMcUe = uintegerValue.Get();
  GlobalValue::GetValueByName ("N_AntennasMmWave", uintegerValue);
  int numAntennasMmWave = uintegerValue.Get();

  Config::SetDefault("ns3::McUeNetDevice::AntennaNum", UintegerValue(numAntennasMcUe));
  Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(16)); // Increase antennas
  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bandwidth));
  Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(centerFrequency));

  Ptr <MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
  
  if (useFriis)
  {
      NS_LOG_UNCOND("Using Friis Propagation Model");
      mmwaveHelper->SetPathlossModelType("ns3::FriisPropagationLossModel");
      mmwaveHelper->SetChannelModelType(""); // Disable spectrum model
  }
  else
  {
      NS_LOG_UNCOND("Using 3GPP UMi Street Canyon Propagation Model");
      mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
      // Default ChannelModel is ThreeGppSpectrumPropagationLossModel
  } 
  
  // Use DFT Beamforming
  mmwaveHelper->SetBeamformingModelType("ns3::MmWaveDftBeamforming");

  // Configure Phased Array (default UniformPlanarArray supports DowntiltAngle)
  mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumRows", UintegerValue(4));
  mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumColumns", UintegerValue(4));
  
  // Apply Mechanical Tilt (converted to radians)
  double tiltRadians = tilt * M_PI / 180.0;
  mmwaveHelper->SetEnbPhasedArrayModelAttribute("DowntiltAngle", DoubleValue(tiltRadians));
  
  // Configure UE Array
  mmwaveHelper->SetUePhasedArrayModelAttribute("NumRows", UintegerValue(2));
  mmwaveHelper->SetUePhasedArrayModelAttribute("NumColumns", UintegerValue(2));

  // Increase Bandwidth to ensure enough RBs (optional but good)
  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(100e6));

  Ptr <MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
  mmwaveHelper->SetEpcHelper(epcHelper);

  GlobalValue::GetValueByName ("N_MmWaveEnbNodes", uintegerValue);
  uint8_t nMmWaveEnbNodes = uintegerValue.Get ();
  GlobalValue::GetValueByName ("N_LteEnbNodes", uintegerValue);
  uint8_t nLteEnbNodes = uintegerValue.Get();
  GlobalValue::GetValueByName ("N_Ues", uintegerValue);
  uint32_t ues = uintegerValue.Get ();
  uint8_t nUeNodes = ues;

  Ptr <Node> pgw = epcHelper->GetPgwNode();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr <Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
  NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr <Ipv4StaticRouting> remoteHostStaticRouting =
      ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer mmWaveEnbNodes;
  NodeContainer lteEnbNodes;
  NodeContainer allEnbNodes;
  mmWaveEnbNodes.Create(nMmWaveEnbNodes);
  lteEnbNodes.Create(nLteEnbNodes);
  ueNodes.Create(nUeNodes);
  allEnbNodes.Add(lteEnbNodes);
  allEnbNodes.Add(mmWaveEnbNodes);

  NodeContainerManager::GetInstance().SetMmWaveEnbNodes(mmWaveEnbNodes);

  Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);

  Ptr <ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
  // Place 2 mmWave nodes separated by isd_cell
  // For 2 nodes, we can place them at -isd_cell/2 and +isd_cell/2 from center
  enbPositionAlloc->Add(Vector(centerPosition.x - isd_cell/2, centerPosition.y, 3));
  enbPositionAlloc->Add(Vector(centerPosition.x + isd_cell/2, centerPosition.y, 3));

  MobilityHelper enbmobility;
  enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbmobility.SetPositionAllocator(enbPositionAlloc);
  enbmobility.Install(allEnbNodes);



  // Install Mobility for UEs (Uniformly distributed between the two gNBs)
  Ptr<ListPositionAllocator> uePositionAllocNew = CreateObject<ListPositionAllocator>();

  // Use Uniform Random Variable to spread UEs between gNB1 (750) and gNB2 (1250)
  Ptr<UniformRandomVariable> xPos = CreateObject<UniformRandomVariable>();
  xPos->SetAttribute("Min", DoubleValue(750.0));
  xPos->SetAttribute("Max", DoubleValue(1250.0));

  // Add some jitter in Y dimension (1000 +/- 20m) to avoid perfect line
  Ptr<UniformRandomVariable> yPos = CreateObject<UniformRandomVariable>();
  yPos->SetAttribute("Min", DoubleValue(980.0));
  yPos->SetAttribute("Max", DoubleValue(1020.0));

  for (uint32_t i = 0; i < ues; i++) {
      uePositionAllocNew->Add(Vector(xPos->GetValue(), yPos->GetValue(), 1.5));
  }

  MobilityHelper uemobility;
  uemobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  uemobility.SetPositionAllocator(uePositionAllocNew);
  uemobility.Install(ueNodes);

  NetDeviceContainer lteEnbDevs = mmwaveHelper->InstallLteEnbDevice(lteEnbNodes);

  NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
  for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
      Ptr <Node> ueNode = ueNodes.Get(u);
      Ptr <Ipv4StaticRouting> ueStaticRouting =
          ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
      ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

  // No X2 interface needed for SA with 0 LTE nodes, or add between mmWave nodes if needed
  // mmwaveHelper->AddX2Interface(mmWaveEnbNodes); // If supported for mmWave-mmWave X2

  // Manual attachment
  // Since we have no LTE, we attach to closest mmWave eNB
  mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs);

  BasicEnergySourceHelper basicEnergySourceHelper;
  basicEnergySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (1000000000000));
  basicEnergySourceHelper.Set ("BasicEnergySupplyVoltageV", DoubleValue (5.0));
  energy::EnergySourceContainer sources = basicEnergySourceHelper.Install (mmWaveEnbNodes);
  MmWaveRadioEnergyModelEnbHelper nrEnbHelper;
  energy::DeviceEnergyModelContainer deviceEModel = nrEnbHelper.Install (mmWaveEnbDevs, sources);

  GlobalValue::GetValueByName ("simTime", doubleValue);
  double simTime = doubleValue.Get ();
  int numPrints = simTime / 0.1;

  for (int x = 0; x < nMmWaveEnbNodes; ++x)
    {
      std::ostringstream filename;
      filename << "energyfilecell" << x + 2 << ".csv";
      deviceEModel.Get (x)->TraceConnectWithoutContext (
          "TotalEnergyConsumption",
          MakeBoundCallback (&EnergyConsumptionUpdate, x, filename.str ()));
      for (int i = 0; i < numPrints; i++)
        {
          Simulator::Schedule (Seconds (i * simTime / numPrints), &EnergyConsumptionPrint, x);
        }
    }

  uint16_t portUdp = 60000;
  Address sinkLocalAddressUdp(InetSocketAddress(Ipv4Address::GetAny(), portUdp));
  PacketSinkHelper sinkHelperUdp("ns3::UdpSocketFactory", sinkLocalAddressUdp);
  AddressValue serverAddressUdp(InetSocketAddress(remoteHostAddr, portUdp));

  ApplicationContainer sinkApp;
  sinkApp.Add(sinkHelperUdp.Install(remoteHost));

  ApplicationContainer clientApp;

  for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
      // Video Streaming Traffic
      PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                           InetSocketAddress(Ipv4Address::GetAny(), 1234));
      sinkApp.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));
      UdpClientHelper dlClient(ueIpIface.GetAddress(u), 1234);
      // ~5 Mbps with 1024 byte packets -> ~610 packets/sec -> ~1639 us interval
      dlClient.SetAttribute("Interval", TimeValue(MicroSeconds(1639)));
      dlClient.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
      dlClient.SetAttribute("PacketSize", UintegerValue(1024)); 
      clientApp.Add(dlClient.Install(remoteHost));
    }

  sinkApp.Start (Seconds (0));
  clientApp.Start(MilliSeconds(100));
  clientApp.Stop(Seconds(simTime - 0.1));

  if (enableTraces)
  {
    // Selective traces - exclude RxPacketTrace to save disk space (~450 MB per run)
    // while keeping PDCP/RLC stats needed for DRB metrics (Scenarios 1 & 2)
    mmwaveHelper->EnableRlcTraces();      // Packet loss, reordering for xAPP3
    mmwaveHelper->EnablePdcpTraces();     // DRB.PdcpSduDelayDl for Scenario 2
    mmwaveHelper->EnableEnbSchedTrace();  // MAC scheduling info
    // Skip: EnableDlPhyTrace() - contains RxPacketTrace (huge disk usage)
    // Skip: EnableUlPhyTrace() - UL packet trace
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
