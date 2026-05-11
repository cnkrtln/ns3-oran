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
#include <algorithm>
#include <iomanip>
#include <sstream>
#include "ns3/basic-energy-source-helper.h"
#include "ns3/mmwave-radio-energy-model-enb-helper.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/propagation-module.h"
#include "ns3/antenna-module.h"
#include "ns3/spectrum-module.h"
#include <ns3/two-ray-spectrum-propagation-loss-model.h>
#include <ns3/channel-condition-model.h>
#include "ns3/mmwave-ue-net-device.h"
#include "ns3/mmwave-enb-net-device.h"
#include "ns3/mmwave-enb-phy.h"
#include "ns3/mmwave-spectrum-phy.h"
#include "ns3/uniform-planar-array.h"
#include <cstdio>

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
double totalnewEnergyConsumption_storage[10] = {0};
double totaloldEnergyConsumption_storage[10] = {0};
double current_energy_consumption[10] = {0};

// Global file stream for UE position export
static std::ofstream g_uePositionFile;
// Runtime flag (derived from GlobalValue g_exportUEPositionsFlag)
static bool g_exportUEPositionsEnabled = false;

NS_LOG_COMPONENT_DEFINE ("DifferingPowerScenario");

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

// Function to log UE and BS positions every 100ms
void
LogUEPositions (NodeContainer& ueNodes, NetDeviceContainer& ueDevs,
                NodeContainer& enbNodes, NetDeviceContainer& enbDevs)
{
  if (!g_exportUEPositionsEnabled || !g_uePositionFile.is_open())
  {
    return;
  }

  double currentTime = Simulator::Now().GetSeconds();
  
  // Log BS positions (static, but log once per frame for visualization)
  for (uint32_t i = 0; i < enbNodes.GetN(); ++i)
  {
    Ptr<Node> enbNode = enbNodes.Get(i);
    Ptr<MmWaveEnbNetDevice> enbDev = enbDevs.Get(i)->GetObject<MmWaveEnbNetDevice>();
    if (!enbDev)
    {
      continue;
    }
    
    Ptr<MobilityModel> enbMobility = enbNode->GetObject<MobilityModel>();
    if (!enbMobility)
    {
      continue;
    }
    
    Vector bsPos = enbMobility->GetPosition();
    uint16_t cellId = enbDev->GetCellId();
    
    g_uePositionFile << std::fixed << std::setprecision(3)
                     << currentTime << ",BS," << cellId << ","
                     << bsPos.x << "," << bsPos.y << "," << bsPos.z << "," << std::endl;
  }
  
  // Log all UE positions
  for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
  {
    Ptr<Node> ueNode = ueNodes.Get(u);
    Ptr<NetDevice> ueDev = ueDevs.Get(u);
    Ptr<MobilityModel> ueMobility = ueNode->GetObject<MobilityModel>();
    
    if (!ueMobility)
    {
      continue;
    }
    
    Vector uePos = ueMobility->GetPosition();
    
    // Get serving cell ID and IMSI
    uint16_t servingCellId = 0;
    uint64_t imsi = 0;
    
    Ptr<MmWaveUeNetDevice> mmWaveUeDev = DynamicCast<MmWaveUeNetDevice>(ueDev);
    if (mmWaveUeDev)
    {
      imsi = mmWaveUeDev->GetImsi();
      Ptr<MmWaveEnbNetDevice> targetEnb = mmWaveUeDev->GetTargetEnb();
      if (targetEnb)
      {
        servingCellId = targetEnb->GetCellId();
      }
    }
    
    g_uePositionFile << std::fixed << std::setprecision(3)
                     << currentTime << ",UE," << imsi << ","
                     << uePos.x << "," << uePos.y << "," << uePos.z << ","
                     << servingCellId << std::endl;
  }
  
  // Schedule next logging (100ms = 0.1s)
  Simulator::Schedule(Seconds(0.1), &LogUEPositions, 
                      ueNodes, ueDevs, enbNodes, enbDevs);
}

// *** Runtime Control Helper Functions ***

/**
 * Change TxPower of a specific cell at runtime
 * @param enbNetDevice Pointer to MmWaveEnbNetDevice
 * @param newPower New TxPower in dBm
 * @param cellId Cell ID for logging
 * @param minPower Minimum allowed power (for validation)
 * @param maxPower Maximum allowed power (for validation)
 */
void ChangeCellTxPower(Ptr<MmWaveEnbNetDevice> enbNetDevice, double newPower, uint16_t cellId,
                       double minPower, double maxPower)
{
  if (!enbNetDevice)
  {
    NS_LOG_ERROR("ChangeCellTxPower: Invalid device pointer");
    return;
  }
  
  // Validate power range
  if (newPower < minPower || newPower > maxPower)
  {
    NS_LOG_WARN("ChangeCellTxPower: Power " << newPower << " dBm out of range [" 
                << minPower << ", " << maxPower << "] for Cell " << cellId);
    return;
  }
  
  Ptr<MmWaveEnbPhy> phy = enbNetDevice->GetPhy();
  if (!phy)
  {
    NS_LOG_ERROR("ChangeCellTxPower: Failed to get PHY for Cell " << cellId);
    return;
  }
  
  // Set the new power value
  phy->SetTxPower(newPower);
  
  // CRITICAL: Update the Power Spectral Density (PSD) that's actually used for transmission
  // SetTxPower only updates the internal variable, but doesn't update the PSD
  Ptr<MmWaveSpectrumPhy> spectrumPhy = phy->GetDlSpectrumPhy();
  if (spectrumPhy)
  {
    // Create new PSD with updated power (uses current subchannels)
    Ptr<SpectrumValue> txPsd = phy->CreateTxPowerSpectralDensity();
    if (txPsd)
    {
      spectrumPhy->SetTxPowerSpectralDensity(txPsd);
      NS_LOG_UNCOND("[" << Simulator::Now().GetSeconds() << "s] Cell " << cellId 
                        << " TxPower changed to " << newPower << " dBm (PSD updated)");
    }
    else
    {
      NS_LOG_WARN("ChangeCellTxPower: Failed to create PSD for Cell " << cellId);
    }
  }
  else
  {
    NS_LOG_WARN("ChangeCellTxPower: Failed to get SpectrumPhy for Cell " << cellId 
                << " (power set but PSD not updated)");
  }
}

/**
 * Change E-Tilt (DowntiltAngle) of a specific cell at runtime
 * @param enbNetDevice Pointer to MmWaveEnbNetDevice
 * @param newTiltDegrees New tilt angle in degrees
 * @param cellId Cell ID for logging
 * @param minTilt Minimum allowed tilt (for validation)
 * @param maxTilt Maximum allowed tilt (for validation)
 */
void ChangeCellTilt(Ptr<MmWaveEnbNetDevice> enbNetDevice, double newTiltDegrees, uint16_t cellId,
                    double minTilt, double maxTilt)
{
  if (!enbNetDevice)
  {
    NS_LOG_ERROR("ChangeCellTilt: Invalid device pointer");
    return;
  }
  
  // Validate tilt range
  if (newTiltDegrees < minTilt || newTiltDegrees > maxTilt)
  {
    NS_LOG_WARN("ChangeCellTilt: Tilt " << newTiltDegrees << " degrees out of range [" 
                << minTilt << ", " << maxTilt << "] for Cell " << cellId);
    return;
  }
  
  Ptr<MmWaveEnbPhy> phy = enbNetDevice->GetPhy();
  if (!phy)
  {
    NS_LOG_ERROR("ChangeCellTilt: Failed to get PHY for Cell " << cellId);
    return;
  }
  
  Ptr<MmWaveSpectrumPhy> spectrumPhy = phy->GetDlSpectrumPhy();
  if (!spectrumPhy)
  {
    NS_LOG_ERROR("ChangeCellTilt: Failed to get SpectrumPhy for Cell " << cellId);
    return;
  }
  
  Ptr<MmWaveBeamformingModel> bfModel = spectrumPhy->GetBeamformingModel();
  if (!bfModel)
  {
    NS_LOG_ERROR("ChangeCellTilt: Failed to get BeamformingModel for Cell " << cellId);
    return;
  }
  
  Ptr<PhasedArrayModel> antenna = bfModel->GetAntenna();
  if (!antenna)
  {
    NS_LOG_ERROR("ChangeCellTilt: Failed to get Antenna for Cell " << cellId);
    return;
  }
  
  Ptr<UniformPlanarArray> upa = DynamicCast<UniformPlanarArray>(antenna);
  if (!upa)
  {
    NS_LOG_ERROR("ChangeCellTilt: Antenna is not UniformPlanarArray for Cell " << cellId);
    return;
  }
  
  double newTiltRadians = newTiltDegrees * M_PI / 180.0;
  upa->SetAttribute("DowntiltAngle", DoubleValue(newTiltRadians));
  NS_LOG_UNCOND("[" << Simulator::Now().GetSeconds() << "s] Cell " << cellId 
                    << " E-Tilt changed to " << newTiltDegrees << " degrees (" 
                    << newTiltRadians << " radians)");
}

/**
 * Check control file for external commands
 * Format: <COMMAND> <CELL_ID> <VALUE>
 * 
 * Commands:
 *   POWER <cellId> <power_dBm>     - Change TxPower
 *   TILT <cellId> <tilt_degrees>   - Change E-Tilt
 * 
 * The file is deleted after reading to avoid re-execution.
 */
void CheckControlFile(Ptr<MmWaveEnbNetDevice> enbDev1, Ptr<MmWaveEnbNetDevice> enbDev2,
                      double txPowerMin, double txPowerMax, double tiltMin, double tiltMax,
                      uint32_t pollIntervalMs)
{
  std::string controlFile = "runtime_control.txt";
  std::ifstream file(controlFile);
  
  if (file.good())
  {
    std::string command;
    int cellId;
    double value;
    
    if (file >> command >> cellId >> value)
    {
      // Validate cell ID
      if (cellId != 1 && cellId != 2)
      {
        NS_LOG_WARN("Invalid cell ID: " << cellId << " (must be 1 or 2)");
        file.close();
        std::remove(controlFile.c_str());
        Simulator::Schedule(MilliSeconds(pollIntervalMs), &CheckControlFile, enbDev1, enbDev2,
                           txPowerMin, txPowerMax, tiltMin, tiltMax, pollIntervalMs);
        return;
      }
      
      // Get target device
      Ptr<MmWaveEnbNetDevice> targetDev = (cellId == 1) ? enbDev1 : enbDev2;
      if (!targetDev)
      {
        NS_LOG_ERROR("Invalid device pointer for Cell " << cellId);
        file.close();
        std::remove(controlFile.c_str());
        Simulator::Schedule(MilliSeconds(pollIntervalMs), &CheckControlFile, enbDev1, enbDev2,
                           txPowerMin, txPowerMax, tiltMin, tiltMax, pollIntervalMs);
        return;
      }
      
      uint16_t actualCellId = targetDev->GetCellId();
      
      // Execute command
      if (command == "POWER" || command == "power" || command == "P")
      {
        ChangeCellTxPower(targetDev, value, actualCellId, txPowerMin, txPowerMax);
      }
      else if (command == "TILT" || command == "tilt" || command == "T")
      {
        ChangeCellTilt(targetDev, value, actualCellId, tiltMin, tiltMax);
      }
      else
      {
        NS_LOG_WARN("Unknown command: " << command << " (valid: POWER, TILT)");
      }
    }
    else
    {
      NS_LOG_WARN("Failed to parse control file. Format: <COMMAND> <CELL_ID> <VALUE>");
    }
    
    file.close();
    
    // Delete file after reading to avoid re-execution
    if (std::remove(controlFile.c_str()) != 0)
    {
      NS_LOG_WARN("Failed to delete control file: " << controlFile);
    }
  }
  
  // Schedule next check (configurable interval)
  Simulator::Schedule(MilliSeconds(pollIntervalMs), &CheckControlFile, enbDev1, enbDev2,
                     txPowerMin, txPowerMax, tiltMin, tiltMax, pollIntervalMs);
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

// *** NEW: Differential TxPower Parameters ***
static ns3::GlobalValue g_txPower1("TxPower1", "Transmission Power for Cell 1 in dBm",
                                  ns3::DoubleValue(46.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_txPower2("TxPower2", "Transmission Power for Cell 2 in dBm",
                                  ns3::DoubleValue(46.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_tilt("Tilt", "Antenna Downtilt in degrees",
                               ns3::DoubleValue(0.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_useFriis("useFriis", "Use Friis Propagation Model (Faster) instead of 3GPP",
                                   ns3::BooleanValue(false), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_useHybrid("useHybrid", "Use Hybrid Propagation Model (LogDistance + Random shadowing) with 3GPP Antenna",
                                    ns3::BooleanValue(false), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_enableTiltTwoRay("enableTiltTwoRay", 
                                           "If true, enable TwoRaySpectrumPropagationLossModel for E-Tilt support in Hybrid mode (default: false for speed)",
                                           ns3::BooleanValue(false), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_tReorderingTimer("tReorderingTimer", "RLC UM t-Reordering timer in milliseconds",
                                           ns3::DoubleValue(35.0), ns3::MakeDoubleChecker<double>());


static ns3::GlobalValue g_mobility("Mobility", "Enable UE Mobility (RandomWalk2d)",
                                   ns3::BooleanValue(false), ns3::MakeBooleanChecker());

static ns3::GlobalValue g_rngRun("RngRun", "Rng Run ID",
                                 ns3::UintegerValue(1), ns3::MakeUintegerChecker<uint32_t>());

// Control flag (CLI / GlobalValue) for UE position export
static ns3::GlobalValue g_exportUEPositionsFlag(
    "exportUEPositions",
    "If true, export UE positions every 100ms to UEPosition.txt (in build directory)",
    ns3::BooleanValue(false),
    ns3::MakeBooleanChecker());

// *** Runtime Control: Configurable Action Spaces ***
static ns3::GlobalValue g_enableRuntimeControl("enableRuntimeControl",
                                               "If true, enable external runtime control via runtime_control.txt",
                                               ns3::BooleanValue(true),
                                               ns3::MakeBooleanChecker());

static ns3::GlobalValue g_txPowerMin("TxPowerMin", "Minimum TxPower for action space (dBm)",
                                     ns3::DoubleValue(0.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_txPowerMax("TxPowerMax", "Maximum TxPower for action space (dBm)",
                                     ns3::DoubleValue(50.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_tiltMin("TiltMin", "Minimum E-Tilt for action space (degrees)",
                                  ns3::DoubleValue(-45.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_tiltMax("TiltMax", "Maximum E-Tilt for action space (degrees)",
                                  ns3::DoubleValue(45.0), ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_controlPollInterval("ControlPollInterval",
                                               "Control file polling interval in milliseconds (simulation time)",
                                               ns3::UintegerValue(10),  // Default: 10ms for better responsiveness
                                               ns3::MakeUintegerChecker<uint32_t>());

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

  GlobalValue::GetValueByName("RngRun", uintegerValue);
  uint32_t rngRun = uintegerValue.Get();
  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(rngRun);

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
  GlobalValue::GetValueByName("useHybrid", booleanValue);
  bool useHybrid = booleanValue.Get();
  GlobalValue::GetValueByName("exportUEPositions", booleanValue);
  bool exportUEPositions = booleanValue.Get();
  
  // Get runtime control configuration
  GlobalValue::GetValueByName("enableRuntimeControl", booleanValue);
  bool enableRuntimeControl = booleanValue.Get();
  GlobalValue::GetValueByName("TxPowerMin", doubleValue);
  double txPowerMin = doubleValue.Get();
  GlobalValue::GetValueByName("TxPowerMax", doubleValue);
  double txPowerMax = doubleValue.Get();
  GlobalValue::GetValueByName("TiltMin", doubleValue);
  double tiltMin = doubleValue.Get();
  GlobalValue::GetValueByName("TiltMax", doubleValue);
  double tiltMax = doubleValue.Get();
  GlobalValue::GetValueByName("ControlPollInterval", uintegerValue);
  uint32_t controlPollIntervalMs = uintegerValue.Get();

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

  // Get TxPower1, TxPower2, and Tilt
  GlobalValue::GetValueByName("TxPower1", doubleValue);
  double txPower1 = doubleValue.Get();
  GlobalValue::GetValueByName("TxPower2", doubleValue);
  double txPower2 = doubleValue.Get();

  GlobalValue::GetValueByName("Tilt", doubleValue);
  double tilt = doubleValue.Get();
  GlobalValue::GetValueByName("tReorderingTimer", doubleValue);
  double tReorderingMs = doubleValue.Get();

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

  // Configure antenna element: ThreeGppAntennaModel for Hybrid mode, IsotropicAntennaModel otherwise
  // Note: ThreeGppAntennaModel has fixed 65° beamwidth (per 3GPP TR 38.901) - no settable attributes
  if (useHybrid)
  {
      Ptr<ThreeGppAntennaModel> threeGppAntenna = CreateObject<ThreeGppAntennaModel>();
      Config::SetDefault("ns3::PhasedArrayModel::AntennaElement", PointerValue(threeGppAntenna));
      NS_LOG_UNCOND("Using ThreeGppAntennaModel (65° vertical and horizontal beamwidth - fixed per 3GPP TR 38.901)");
  }
  else
  {
      Config::SetDefault("ns3::PhasedArrayModel::AntennaElement",
                         PointerValue(CreateObject<IsotropicAntennaModel>()));
  }
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

  Config::SetDefault("ns3::LteRlcUm::ReorderingTimer", TimeValue(MilliSeconds(tReorderingMs)));
  NS_LOG_UNCOND("t-Reordering Timer set to: " << tReorderingMs << " ms");

  Config::SetDefault("ns3::LteEnbRrc::OutageThreshold", DoubleValue(outageThreshold));
  Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue(handoverMode));
  Config::SetDefault("ns3::LteEnbRrc::HoSinrDifference", DoubleValue(hoSinrDifference));
  Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency",DoubleValue(3.5e9));
  // Shadowing: enabled for 3GPP, disabled for Hybrid (handled by RandomPropagationLossModel)
  Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled",BooleanValue(!useHybrid));
  
  NS_LOG_UNCOND("Differing Power Scenario Parameters:");
  NS_LOG_UNCOND("  TxPower Cell 1: " << txPower1 << " dBm");
  NS_LOG_UNCOND("  TxPower Cell 2: " << txPower2 << " dBm");
  NS_LOG_UNCOND("  Tilt: " << tilt << " degrees");
  if (useHybrid)
  {
      NS_LOG_UNCOND("  Propagation: Hybrid (LogDistance + Random Shadowing)");
      NS_LOG_UNCOND("  Antenna: ThreeGppAntennaModel (65° beamwidth)");
      NS_LOG_UNCOND("  BS Height: 15.0 m");
  }
  else if (useFriis)
  {
      NS_LOG_UNCOND("  Propagation: Friis");
  }
  else
  {
      NS_LOG_UNCOND("  Propagation: 3GPP UMi Street Canyon");
  }

  GlobalValue::GetValueByName ("Bandwidth", doubleValue);
  double bandwidth = doubleValue.Get ();
  GlobalValue::GetValueByName ("CenterFrequency", doubleValue);
  double centerFrequency = doubleValue.Get ();
  // GlobalValue::GetValueByName ("IntersideDistanceUEs", doubleValue);
  // double isd_ue = doubleValue.Get (); 
  GlobalValue::GetValueByName ("IntersideDistanceCells", doubleValue);
  double isd_cell = doubleValue.Get (); 

  GlobalValue::GetValueByName ("N_AntennasMcUe", uintegerValue);
  int numAntennasMcUe = uintegerValue.Get();
  // GlobalValue::GetValueByName ("N_AntennasMmWave", uintegerValue);
  // int numAntennasMmWave = uintegerValue.Get();

  Config::SetDefault("ns3::McUeNetDevice::AntennaNum", UintegerValue(numAntennasMcUe));
  Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(16)); // Increase antennas
  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bandwidth));
  Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(centerFrequency));

  Ptr <MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
  
  if (useHybrid)
  {
      BooleanValue enableTiltTwoRayValue;
      GlobalValue::GetValueByName("enableTiltTwoRay", enableTiltTwoRayValue);
      bool enableTiltTwoRay = enableTiltTwoRayValue.Get();
      
      if (enableTiltTwoRay)
      {
          NS_LOG_UNCOND("Using Hybrid Propagation Model (LogDistance + Random Shadowing + TwoRay for E-Tilt)");
          mmwaveHelper->SetPathlossModelType("ns3::LogDistancePropagationLossModel");
          
          // Configure LogDistance attributes (unchanged - TxPower still works via this)
          Config::SetDefault("ns3::LogDistancePropagationLossModel::Exponent", DoubleValue(3.8));
          Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue(43.3));
          
          // Enable TwoRaySpectrumPropagationLossModel for phased array antenna gain (includes E-Tilt)
          // Set ChannelConditionModelType (mmwaveHelper will create it, but we still need to set it on TwoRay)
          mmwaveHelper->SetChannelConditionModelType("ns3::AlwaysLosChannelConditionModel");
          
          // Set model type FIRST, then attributes
          mmwaveHelper->SetChannelModelType("ns3::TwoRaySpectrumPropagationLossModel");
          
          // Set attributes on TwoRay model
          // Frequency: use centerFrequency from scenario (will be set by mmwaveHelper later, but we set a default)
          // Note: mmwaveHelper will override this with phyMacCommon->GetCenterFrequency() during initialization
          mmwaveHelper->SetChannelModelAttribute("Frequency", DoubleValue(centerFrequency));
          // Scenario: UMi-StreetCanyon (common urban scenario, pre-calibrated)
          mmwaveHelper->SetChannelModelAttribute("Scenario", StringValue("UMi-StreetCanyon"));
          
          // ChannelConditionModel: Create and set via factory attribute (smart pointer will keep it alive)
          // Note: mmwaveHelper's else branch doesn't automatically associate ChannelConditionModel to spectrum model
          // like it does for ThreeGpp, so we need to set it explicitly
          Ptr<ChannelConditionModel> losModel = CreateObject<AlwaysLosChannelConditionModel>();
          mmwaveHelper->SetChannelModelAttribute("ChannelConditionModel", PointerValue(losModel));
      }
      else
      {
          NS_LOG_UNCOND("Using Hybrid Propagation Model (LogDistance + Random Shadowing)");
          mmwaveHelper->SetPathlossModelType("ns3::LogDistancePropagationLossModel");
          mmwaveHelper->SetChannelModelType(""); // Disable heavy 3GPP spectrum model
          // Configure LogDistance attributes
          Config::SetDefault("ns3::LogDistancePropagationLossModel::Exponent", DoubleValue(3.8));
          Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue(43.3));
      }
  }
  else if (useFriis)
  {
      NS_LOG_UNCOND("Using Friis Propagation Model");
      mmwaveHelper->SetPathlossModelType("ns3::FriisPropagationLossModel");
      mmwaveHelper->SetChannelModelType(""); // Disable spectrum model
  }
  else
  {
      NS_LOG_UNCOND("Using 3GPP UMi Street Canyon Propagation Model");
      mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
  } 
  
  mmwaveHelper->SetBeamformingModelType("ns3::MmWaveDftBeamforming");

  mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumRows", UintegerValue(16));
  mmwaveHelper->SetEnbPhasedArrayModelAttribute("NumColumns", UintegerValue(4));
  
  double tiltRadians = tilt * M_PI / 180.0;
  mmwaveHelper->SetEnbPhasedArrayModelAttribute("DowntiltAngle", DoubleValue(tiltRadians));
  
  mmwaveHelper->SetUePhasedArrayModelAttribute("NumRows", UintegerValue(2));
  mmwaveHelper->SetUePhasedArrayModelAttribute("NumColumns", UintegerValue(2));

  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(100e6));

  Ptr <MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
  mmwaveHelper->SetEpcHelper(epcHelper);

  // We enforce 2 nodes for this scenario for simplicity
  uint8_t nMmWaveEnbNodes = 2; 

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
  
  // Create nodes
  mmWaveEnbNodes.Create(nMmWaveEnbNodes);
  lteEnbNodes.Create(nLteEnbNodes);
  ueNodes.Create(nUeNodes);

  // Split into individual containers for separate configuration
  NodeContainer mmWaveEnbNode1 = NodeContainer(mmWaveEnbNodes.Get(0));
  NodeContainer mmWaveEnbNode2 = NodeContainer(mmWaveEnbNodes.Get(1));

  NodeContainer allEnbNodes;
  allEnbNodes.Add(lteEnbNodes);
  allEnbNodes.Add(mmWaveEnbNodes);

  NodeContainerManager::GetInstance().SetMmWaveEnbNodes(mmWaveEnbNodes);

  Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);

  // Base station height: 15.0m for Hybrid mode (realistic), 3.0m otherwise
  double enbHeight = useHybrid ? 5.0 : 3.0;
  Ptr <ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
  enbPositionAlloc->Add(Vector(centerPosition.x - isd_cell/2, centerPosition.y, enbHeight));
  enbPositionAlloc->Add(Vector(centerPosition.x + isd_cell/2, centerPosition.y, enbHeight));

  MobilityHelper enbmobility;
  enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbmobility.SetPositionAllocator(enbPositionAlloc);
  enbmobility.Install(allEnbNodes);

  // Install Mobility for UEs (uniformly distributed over the full mobility area)
  Ptr<ListPositionAllocator> uePositionAllocNew = CreateObject<ListPositionAllocator>();
  Ptr<UniformRandomVariable> xPos = CreateObject<UniformRandomVariable>();
  // Match initial placement to mobility bounds in X: [500, 1500]
  xPos->SetAttribute("Min", DoubleValue(500.0));
  xPos->SetAttribute("Max", DoubleValue(1500.0));
  Ptr<UniformRandomVariable> yPos = CreateObject<UniformRandomVariable>();
  // Match initial placement to mobility bounds in Y: [800, 1200]
  yPos->SetAttribute("Min", DoubleValue(800.0));
  yPos->SetAttribute("Max", DoubleValue(1200.0));

  for (uint32_t i = 0; i < ues; i++) {
      uePositionAllocNew->Add(Vector(xPos->GetValue(), yPos->GetValue(), 1.5));
  }

  GlobalValue::GetValueByName("Mobility", booleanValue);
  bool mobilityEnabled = booleanValue.Get();

  MobilityHelper uemobility;
  if (!mobilityEnabled)
    {
      // Legacy behavior: all UEs static at their initial positions
      uemobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
      uemobility.SetPositionAllocator(uePositionAllocNew);
      uemobility.Install(ueNodes);
    }
  else
    {
      // Heterogeneous mobility profile:
      // Group 1: static
      // Group 2: pedestrians ~1.5 m/s
      // Group 3: cyclists ~6.0 m/s
      // Group 4: cars ~20.0 m/s
      // Groups are sized proportionally (roughly N_Ues/4 each).

      // Common bounds for all mobile groups (same as RandomWalk2d used before)
      Rectangle bounds (centerPosition.x - isd_cell,
                        centerPosition.x + isd_cell,
                        centerPosition.y - 200,
                        centerPosition.y + 200);

      // Helpers for each group, all sharing the same initial position allocator
      MobilityHelper mobStatic;
      MobilityHelper mobPed;
      MobilityHelper mobCyclist;
      MobilityHelper mobCar;

      mobStatic.SetPositionAllocator (uePositionAllocNew);
      mobPed.SetPositionAllocator (uePositionAllocNew);
      mobCyclist.SetPositionAllocator (uePositionAllocNew);
      mobCar.SetPositionAllocator (uePositionAllocNew);

      // Group 1: ConstantPosition (0 m/s)
      mobStatic.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

      // Group 2: pedestrians, 1.5 m/s, change direction every 2 s
      mobPed.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (bounds),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.5]"),
                               "Time", TimeValue (Seconds (2.0)));

      // Group 3: cyclists, 6.0 m/s, change direction every 3 s
      mobCyclist.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                   "Bounds", RectangleValue (bounds),
                                   "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=6.0]"),
                                   "Time", TimeValue (Seconds (3.0)));

      // Group 4: cars, 20.0 m/s, change direction every 5 s
      mobCar.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (bounds),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=20.0]"),
                               "Time", TimeValue (Seconds (5.0)));

      // Partition UE nodes into 4 groups proportionally.
      uint32_t totalUes = ueNodes.GetN ();
      // Base size and remainder for roughly equal split
      uint32_t baseSize = totalUes / 4;
      uint32_t rem      = totalUes % 4;

      uint32_t size1 = baseSize + (rem > 0 ? 1 : 0);
      uint32_t size2 = baseSize + (rem > 1 ? 1 : 0);
      uint32_t size3 = baseSize + (rem > 2 ? 1 : 0);
      uint32_t size4 = baseSize; // remainder already distributed

      uint32_t g1End = size1;
      uint32_t g2End = g1End + size2;
      uint32_t g3End = g2End + size3;
      // Remaining (up to totalUes) go to group 4

      NodeContainer group1;
      NodeContainer group2;
      NodeContainer group3;
      NodeContainer group4;

      for (uint32_t i = 0; i < totalUes; ++i)
        {
          Ptr<Node> ueNode = ueNodes.Get (i);
          if (i < g1End)
            {
              group1.Add (ueNode);
            }
          else if (i < g2End)
            {
              group2.Add (ueNode);
            }
          else if (i < g3End)
            {
              group3.Add (ueNode);
            }
          else
            {
              group4.Add (ueNode);
            }
        }

      if (group1.GetN () > 0)
        {
          mobStatic.Install (group1);
        }
      if (group2.GetN () > 0)
        {
          mobPed.Install (group2);
        }
      if (group3.GetN () > 0)
        {
          mobCyclist.Install (group3);
        }
      if (group4.GetN () > 0)
        {
          mobCar.Install (group4);
        }
    }

  NetDeviceContainer lteEnbDevs = mmwaveHelper->InstallLteEnbDevice(lteEnbNodes);

  // *** Differential Power Installation ***
  
  // Install Cell 1
  Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(txPower1));
  NetDeviceContainer mmWaveEnbDev1 = mmwaveHelper->InstallEnbDevice(mmWaveEnbNode1);

  // Install Cell 2
  Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(txPower2));
  NetDeviceContainer mmWaveEnbDev2 = mmwaveHelper->InstallEnbDevice(mmWaveEnbNode2);

  // Merge Device Containers for tracking
  NetDeviceContainer mmWaveEnbDevs;
  mmWaveEnbDevs.Add(mmWaveEnbDev1);
  mmWaveEnbDevs.Add(mmWaveEnbDev2);
  
  // *** Store device pointers for runtime control ***
  Ptr<MmWaveEnbNetDevice> enbDev1 = mmWaveEnbDev1.Get(0)->GetObject<MmWaveEnbNetDevice>();
  Ptr<MmWaveEnbNetDevice> enbDev2 = mmWaveEnbDev2.Get(0)->GetObject<MmWaveEnbNetDevice>();
  
  if (!enbDev1 || !enbDev2)
  {
    NS_FATAL_ERROR("Failed to get MmWaveEnbNetDevice pointers for runtime control");
  }
  
  uint16_t cellId1 = enbDev1->GetCellId();
  uint16_t cellId2 = enbDev2->GetCellId();
  
  // Log action space configuration
  if (enableRuntimeControl)
  {
    NS_LOG_UNCOND("=== Runtime Control Enabled ===");
    NS_LOG_UNCOND("  Control file: runtime_control.txt (polling every " << controlPollIntervalMs << "ms simulation time)");
    NS_LOG_UNCOND("  TxPower action space: [" << txPowerMin << ", " << txPowerMax << "] dBm");
    NS_LOG_UNCOND("  E-Tilt action space: [" << tiltMin << ", " << tiltMax << "] degrees");
    NS_LOG_UNCOND("  Cell " << cellId1 << " -> enbDev1");
    NS_LOG_UNCOND("  Cell " << cellId2 << " -> enbDev2");
    NS_LOG_UNCOND("  Command format: POWER <cellId> <value>  or  TILT <cellId> <value>");
  }

  // *** Hybrid Mode: Chain RandomPropagationLossModel for shadowing ***
  if (useHybrid)
  {
      NS_LOG_UNCOND("Chaining RandomPropagationLossModel for stochastic shadowing");
      // Create NormalRandomVariable for shadowing (Mean=0, Variance=49.0 -> StdDev=7.0 dB)
      Ptr<NormalRandomVariable> shadowingVar = CreateObject<NormalRandomVariable>();
      shadowingVar->SetAttribute("Mean", DoubleValue(0.0));
      shadowingVar->SetAttribute("Variance", DoubleValue(49.0));
      
      // Get the pathloss model for each component carrier (typically index 0)
      Ptr<PropagationLossModel> baseModel = mmwaveHelper->GetPathLossModel(0);
      if (baseModel)
      {
          // Create and configure RandomPropagationLossModel
          Ptr<RandomPropagationLossModel> shadowModel = CreateObject<RandomPropagationLossModel>();
          shadowModel->SetAttribute("Variable", PointerValue(shadowingVar));
          
          // Chain the shadowing model after the LogDistance model
          baseModel->SetNext(shadowModel);
          NS_LOG_UNCOND("Successfully chained RandomPropagationLossModel to LogDistancePropagationLossModel");
      }
      else
      {
          NS_LOG_ERROR("Failed to retrieve pathloss model for chaining - Hybrid mode may not work correctly");
      }
  }

  // Install UEs
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

  // *** Setup X2 interfaces between mmWave eNBs for Standalone (SA) mode handover ***
  // This enables direct mmWave-to-mmWave SINR exchange and handover decisions
  mmwaveHelper->AddX2Interface(mmWaveEnbNodes);
  NS_LOG_UNCOND("X2 interfaces established between mmWave eNBs for SA mode handover");

  // Calculate Distance Helper
  auto CalculateDistance = [](Vector a, Vector b) {
      return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2) + std::pow(a.z - b.z, 2));
  };

  // Custom Attachment: Attach to Best Server (Highest Estimated RxPower)
  // Approximated as: RxPower_dB = TxPower_dBm - 20*log10(Distance)
  for (uint32_t u = 0; u < ueDevs.GetN(); ++u) {
      Ptr<NetDevice> ueDev = ueDevs.Get(u);
      Ptr<Node> ueNode = ueNodes.Get(u);
      Vector uePos = ueNode->GetObject<MobilityModel>()->GetPosition();

      // Check Cell 1
      Ptr<Node> enb1Node = mmWaveEnbNode1.Get(0);
      Vector enb1Pos = enb1Node->GetObject<MobilityModel>()->GetPosition();
      double dist1 = CalculateDistance(uePos, enb1Pos);
      // Protect against dist=0
      double pathloss1 = (dist1 > 0) ? 20 * std::log10(dist1) : 0;
      double estRxPower1 = txPower1 - pathloss1;

      // Check Cell 2
      Ptr<Node> enb2Node = mmWaveEnbNode2.Get(0);
      Vector enb2Pos = enb2Node->GetObject<MobilityModel>()->GetPosition();
      double dist2 = CalculateDistance(uePos, enb2Pos);
      double pathloss2 = (dist2 > 0) ? 20 * std::log10(dist2) : 0;
      double estRxPower2 = txPower2 - pathloss2;

      if (estRxPower1 >= estRxPower2) {
          mmwaveHelper->AttachToEnbWithIndex(ueDev, mmWaveEnbDevs, 0);
      } else {
          mmwaveHelper->AttachToEnbWithIndex(ueDev, mmWaveEnbDevs, 1);
      }
  }

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
      PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                           InetSocketAddress(Ipv4Address::GetAny(), 1234));
      sinkApp.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));
      UdpClientHelper dlClient(ueIpIface.GetAddress(u), 1234);
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
    mmwaveHelper->EnableRlcTraces();      
    mmwaveHelper->EnablePdcpTraces();     
    mmwaveHelper->EnableEnbSchedTrace();  
  }

  // Setup UE position export if enabled
  if (exportUEPositions)
  {
    // Open file in build directory
    std::string positionFile = "UEPosition.txt";
    g_uePositionFile.open(positionFile, std::ios::out);
    
    if (g_uePositionFile.is_open())
    {
      g_exportUEPositionsEnabled = true;
      g_uePositionFile << "Time(s),Type,ID,X(m),Y(m),Z(m),CellID" << std::endl;
      NS_LOG_UNCOND("UE position export enabled. Writing to: " << positionFile);
      
      // Start logging from t=0.1s (first frame after initial setup)
      Simulator::Schedule(Seconds(0.1), &LogUEPositions, 
                          ueNodes, ueDevs, mmWaveEnbNodes, mmWaveEnbDevs);
      
      // Close file at end of simulation
      Simulator::ScheduleDestroy([]() { 
        if (g_uePositionFile.is_open()) {
          g_uePositionFile.close();
          NS_LOG_UNCOND("UE position trace file closed.");
        }
      });
    }
    else
    {
      NS_LOG_ERROR("Failed to open UE position file: " << positionFile);
    }
  }

  // *** Start external runtime control file polling (if enabled) ***
  if (enableRuntimeControl)
  {
    Simulator::Schedule(Seconds(0.1), &CheckControlFile, enbDev1, enbDev2,
                        txPowerMin, txPowerMax, tiltMin, tiltMax, controlPollIntervalMs);
    NS_LOG_UNCOND("Runtime control polling started. Waiting for commands in runtime_control.txt");
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
