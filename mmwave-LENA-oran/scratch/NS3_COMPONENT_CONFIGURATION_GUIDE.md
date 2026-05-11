# NS-3 Component Configuration Guide for Custom Scenarios

## Overview
This guide explains all NS-3 components and configurations needed to create **ANY** network scenario (SA, EN-DC, etc.) and collect KPM (Key Performance Metrics) data via O-RAN E2 interface.

---

## Table of Contents
1. [NS-3 Architecture Components](#1-ns-3-architecture-components)
2. [Scenario Types: SA vs EN-DC](#2-scenario-types-sa-vs-en-dc)
3. [Core Configuration Modules](#3-core-configuration-modules)
4. [Network Topology Configuration](#4-network-topology-configuration)
5. [E2 Interface and KPM Metrics Setup](#5-e2-interface-and-kpm-metrics-setup)
6. [Step-by-Step Configuration Guide](#6-step-by-step-configuration-guide)
7. [Example: 2-Cell 5G SA Setup with 20 UEs](#7-example-2-cell-5g-sa-setup-with-20-ues)
8. [Configuration Parameters Reference](#8-configuration-parameters-reference)

---

## 1. NS-3 Architecture Components

### 1.1 Core NS-3 Modules

#### **Core Module** (`ns3/core-module.h`)
- **Purpose**: Simulation kernel, events, time, attributes
- **Key Features**: Event scheduler, time management, attribute system
- **Usage**: Always included, handles simulation execution

#### **Network Module** (`ns3/network-module.h`)
- **Purpose**: Basic network components (nodes, addresses, packets)
- **Key Features**: Node container, packet, address types
- **Usage**: Node creation and management

#### **Internet Module** (`ns3/internet-module.h`)
- **Purpose**: IP stack, routing, addressing
- **Key Features**: IPv4/IPv6, TCP/UDP, routing protocols
- **Usage**: IP stack installation, routing configuration

#### **Mobility Module** (`ns3/mobility-module.h`)
- **Purpose**: Node positioning and movement
- **Key Features**: Position allocators, mobility models
- **Usage**: Set node positions and mobility patterns

#### **Applications Module** (`ns3/applications-module.h`)
- **Purpose**: Traffic generation (UDP, TCP, etc.)
- **Key Features**: Client-server applications, traffic patterns
- **Usage**: Create traffic sources and sinks

#### **Point-to-Point Module** (`ns3/point-to-point-helper.h`)
- **Purpose**: Point-to-point links
- **Key Features**: P2P links with configurable data rate and delay
- **Usage**: Connect PGW to remote host

### 1.2 LTE/mmWave Specific Modules

#### **mmWave Helper** (`ns3/mmwave-helper.h`)
- **Purpose**: Main helper for mmWave and 5G NR simulation
- **Key Features**: 
  - Device installation (eNB, gNB, UE)
  - Channel model configuration
  - Pathloss model configuration
  - E2 interface integration
- **Usage**: Primary interface for 5G/mmWave simulation setup

#### **EPC Helper** (`ns3/epc-helper.h`, `ns3/mmwave-point-to-point-epc-helper.h`)
- **Purpose**: Evolved Packet Core (EPC) simulation
- **Key Features**:
  - PGW (Packet Data Network Gateway)
  - SGW (Serving Gateway)
  - MME (Mobility Management Entity)
  - IP address assignment
- **Usage**: Core network setup for cellular simulation

#### **LTE Helper** (`ns3/lte-helper.h`)
- **Purpose**: LTE-specific helper functions
- **Key Features**: LTE trace generation, PHY/MAC traces
- **Usage**: Enable LTE traces, LTE-specific configurations

#### **Node Container Manager** (`../src/mmwave/model/node-container-manager.h`)
- **Purpose**: Manage mmWave eNB nodes for E2 interface
- **Key Features**: Node container management
- **Usage**: Required for E2 interface to track nodes

### 1.3 Device Types

#### **LTE eNB Device** (`LteEnbNetDevice`)
- **Purpose**: LTE base station (4G)
- **Usage**: EN-DC scenarios (LTE anchor + mmWave secondary)
- **Configuration**: Cell ID, transmit power, E2 reports

#### **mmWave eNB Device** (`MmWaveEnbNetDevice`)
- **Purpose**: mmWave/5G NR base station (gNB)
- **Usage**: SA scenarios (standalone 5G) or EN-DC scenarios
- **Configuration**: Cell ID, frequency, bandwidth, E2 reports

#### **LTE UE Device** (`LteUeNetDevice`)
- **Purpose**: LTE-only user equipment
- **Usage**: LTE-only scenarios

#### **mmWave UE Device** (`MmWaveUeNetDevice`)
- **Purpose**: mmWave-only user equipment
- **Usage**: mmWave-only scenarios

#### **MC UE Device** (`McUeNetDevice`)
- **Purpose**: Multi-connectivity user equipment (LTE + mmWave)
- **Usage**: EN-DC scenarios (Option 3A)
- **Configuration**: Can connect to both LTE and mmWave cells

---

## 2. Scenario Types: SA vs EN-DC

### 2.1 Standalone (SA) 5G Scenario

**Configuration:**
- **LTE eNBs**: 0
- **mmWave eNBs**: 2+ (gNBs only)
- **UE Devices**: `MmWaveUeNetDevice` (mmWave-only)
- **EPC**: Required (for IP connectivity)
- **X2 Interface**: Between mmWave eNBs (optional)

**Example:**
```cpp
uint8_t nLteEnbNodes = 0;        // No LTE eNBs
uint8_t nMmWaveEnbNodes = 2;     // 2 mmWave gNBs
NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);
NetDeviceContainer mmWaveUeDevs = mmwaveHelper->InstallUeDevice(ueNodes);  // mmWave-only UEs
```

### 2.2 EN-DC (E-UTRAN New Radio - Dual Connectivity) Scenario

**Configuration:**
- **LTE eNBs**: 1+ (anchor)
- **mmWave eNBs**: 1+ (secondary)
- **UE Devices**: `McUeNetDevice` (multi-connectivity)
- **EPC**: Required
- **X2 Interface**: Between LTE and mmWave eNBs (required)

**Example:**
```cpp
uint8_t nLteEnbNodes = 1;        // 1 LTE eNB (anchor)
uint8_t nMmWaveEnbNodes = 2;     // 2 mmWave gNBs (secondary)
NetDeviceContainer lteEnbDevs = mmwaveHelper->InstallLteEnbDevice(lteEnbNodes);
NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);
NetDeviceContainer mcUeDevs = mmwaveHelper->InstallMcUeDevice(ueNodes);  // Multi-connectivity UEs
mmwaveHelper->AddX2Interface(lteEnbNodes, mmWaveEnbNodes);  // X2 interface required
```

### 2.3 Key Differences

| Aspect | SA (Standalone) | EN-DC (Dual Connectivity) |
|--------|-----------------|---------------------------|
| **LTE eNBs** | 0 | 1+ (required) |
| **mmWave eNBs** | 2+ | 1+ |
| **UE Device Type** | `MmWaveUeNetDevice` | `McUeNetDevice` |
| **X2 Interface** | Optional (between gNBs) | Required (LTE ↔ mmWave) |
| **Handover** | Between mmWave cells | Between LTE and mmWave |
| **Use Case** | Pure 5G deployment | 5G deployment with LTE anchor |

---

## 3. Core Configuration Modules

### 3.1 Physical Layer Configuration

#### **Pathloss Model**
```cpp
mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
```
**Options:**
- `ThreeGppUmiStreetCanyonPropagationLossModel` - Urban micro (street canyon)
- `ThreeGppRmaPropagationLossModel` - Rural macro
- `ThreeGppUmaPropagationLossModel` - Urban macro
- `FriisPropagationLossModel` - Free space

#### **Channel Condition Model**
```cpp
mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
```
**Options:**
- `ThreeGppUmiStreetCanyonChannelConditionModel` - Urban micro
- `ThreeGppRmaChannelConditionModel` - Rural macro
- `ThreeGppUmaChannelConditionModel` - Urban macro

#### **Frequency and Bandwidth**
```cpp
Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(20e6));      // 20 MHz
Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(3.5e9));    // 3.5 GHz
Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency", DoubleValue(3.5e9));
```

#### **Antenna Configuration**
```cpp
// Isotropic antenna (simplified)
Config::SetDefault("ns3::PhasedArrayModel::AntennaElement", 
    PointerValue(CreateObject<IsotropicAntennaModel>()));

// Number of antennas
Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(1));
Config::SetDefault("ns3::McUeNetDevice::AntennaNum", UintegerValue(1));
```

#### **Channel Model Update Period**
```cpp
Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(100.0)));
Config::SetDefault("ns3::ThreeGppChannelConditionModel::UpdatePeriod", TimeValue(MilliSeconds(100)));
```

### 3.2 MAC/RLC Layer Configuration

#### **HARQ (Hybrid Automatic Repeat Request)**
```cpp
bool harqEnabled = true;
Config::SetDefault("ns3::MmWaveHelper::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));
```

#### **RLC Buffer Configuration**
```cpp
uint32_t bufferSize = 10;  // MB
Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MilliSeconds(10.0)));
Config::SetDefault("ns3::LteRlcUmLowLat::ReportBufferStatusTimer", TimeValue(MilliSeconds(10.0)));
Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
Config::SetDefault("ns3::LteRlcUmLowLat::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
```

#### **Random Access Configuration**
```cpp
uint8_t numberOfRaPreambles = 40;
Config::SetDefault("ns3::MmWaveEnbMac::NumberOfRaPreambles", UintegerValue(numberOfRaPreambles));
```

#### **RRC Configuration**
```cpp
Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));  // Ideal RRC (no errors)
```

### 3.3 Handover Configuration

#### **Handover Mode**
```cpp
std::string handoverMode = "DynamicTtt";  // Options: "NoAuto", "FixedTtt", "DynamicTtt", "Threshold"
Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue(handoverMode));
```

#### **Handover SINR Difference**
```cpp
double hoSinrDifference = 3.0;  // dB
Config::SetDefault("ns3::LteEnbRrc::HoSinrDifference", DoubleValue(hoSinrDifference));
```

#### **Outage Threshold**
```cpp
double outageThreshold = -5.0;  // dB
Config::SetDefault("ns3::LteEnbRrc::OutageThreshold", DoubleValue(outageThreshold));
```

### 3.4 Shadowing Configuration
```cpp
Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue(false));
// Set to true to enable shadowing (adds randomness to pathloss)
```

---

## 4. Network Topology Configuration

### 4.1 Scenario Dimensions
```cpp
double maxXAxis = 4000;  // meters (simulation area width)
double maxYAxis = 4000;  // meters (simulation area height)
```

### 4.2 Node Counts
```cpp
uint8_t nMmWaveEnbNodes = 2;    // Number of mmWave gNBs
uint8_t nLteEnbNodes = 0;       // Number of LTE eNBs (0 for SA, 1+ for EN-DC)
uint32_t nUeNodes = 20;         // Number of UEs
```

### 4.3 Inter-Site Distance
```cpp
double isd_cell = 500;   // Inter-site distance for cells (meters)
double isd_ue = 1000;    // Radius for UE distribution (meters)
```

### 4.4 Node Positioning

#### **eNB Positioning (Circular Constellation)**
```cpp
Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);  // Center of simulation area

Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();

// For EN-DC: Co-located LTE and mmWave at center
if (nLteEnbNodes > 0) {
    enbPositionAlloc->Add(centerPosition);  // LTE eNB at center
}
enbPositionAlloc->Add(centerPosition);      // First mmWave gNB at center

// Remaining mmWave gNBs in circular constellation
double nConstellation = nMmWaveEnbNodes - 1;
for (int8_t i = 0; i < nConstellation; ++i) {
    double x = isd_cell * cos((2 * M_PI * i) / (nConstellation));
    double y = isd_cell * sin((2 * M_PI * i) / (nConstellation));
    enbPositionAlloc->Add(Vector(centerPosition.x + x, centerPosition.y + y, 3));
}

MobilityHelper enbmobility;
enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
enbmobility.SetPositionAllocator(enbPositionAlloc);
enbmobility.Install(allEnbNodes);
```

#### **UE Positioning (Uniform Disc)**
```cpp
Ptr<UniformDiscPositionAllocator> uePositionAlloc = CreateObject<UniformDiscPositionAllocator>();
uePositionAlloc->SetX(centerPosition.x);
uePositionAlloc->SetY(centerPosition.y);
uePositionAlloc->SetRho(isd_ue);  // Radius

MobilityHelper uemobility;

// Speed configuration
Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
speed->SetAttribute("Min", DoubleValue(2.0));  // 2 m/s
speed->SetAttribute("Max", DoubleValue(4.0));  // 4 m/s

// Mobility model
uemobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel", 
    "Speed", PointerValue(speed),
    "Bounds", RectangleValue(Rectangle(0, maxXAxis, 0, maxYAxis)));

uemobility.SetPositionAllocator(uePositionAlloc);
uemobility.Install(ueNodes);
```

**Mobility Model Options:**
- `ConstantPositionMobilityModel` - Fixed position
- `RandomWalk2dOutdoorMobilityModel` - Random walk (2D)
- `RandomWaypointMobilityModel` - Random waypoint
- `ConstantVelocityMobilityModel` - Constant velocity
- `GaussMarkovMobilityModel` - Gauss-Markov

---

## 5. E2 Interface and KPM Metrics Setup

### 5.1 E2 Interface Configuration

#### **E2 Periodicity (Report Interval)**
```cpp
double indicationPeriodicity = 0.1;  // seconds (100 ms)
Config::SetDefault("ns3::LteEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
```

#### **E2 Mode (LTE vs NR)**
```cpp
bool e2lteEnabled = true;   // Enable LTE E2 reports (for EN-DC)
bool e2nrEnabled = true;    // Enable NR E2 reports (for SA or EN-DC)
Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));
Config::SetDefault("ns3::MmWaveHelper::E2ModeNr", BooleanValue(e2nrEnabled));
```

#### **E2 Report Types (DU, CU-UP, CU-CP)**
```cpp
bool e2du = true;     // DU (Distributed Unit) reports - PHY, MAC, RLC
bool e2cuUp = true;   // CU-UP (Central Unit - User Plane) reports - PDCP
bool e2cuCp = true;   // CU-CP (Central Unit - Control Plane) reports - RRC

// DU reports (from both LTE and mmWave eNBs)
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(e2du));

// CU-UP reports (from LTE eNB in EN-DC, or mmWave eNB in SA)
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
Config::SetDefault("ns3::LteEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));

// CU-CP reports (from LTE eNB in EN-DC, or mmWave eNB in SA)
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));
Config::SetDefault("ns3::LteEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));
```

#### **E2 Function IDs**
```cpp
double g_e2_func_id = 2;      // KPM (Key Performance Metrics) function ID
double g_rc_e2_func_id = 3;   // RC (RAN Control) function ID

Config::SetDefault("ns3::LteEnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));
Config::SetDefault("ns3::MmWaveEnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));
Config::SetDefault("ns3::LteEnbNetDevice::RC_E2functionID", DoubleValue(g_rc_e2_func_id));
Config::SetDefault("ns3::MmWaveEnbNetDevice::RC_E2functionID", DoubleValue(g_rc_e2_func_id));
```

#### **E2 Termination IP (RIC Connection)**
```cpp
std::string e2TermIp = "127.0.0.1";  // RIC (RAN Intelligent Controller) IP address
Config::SetDefault("ns3::MmWaveHelper::E2TermIp", StringValue(e2TermIp));
```

#### **E2 File Logging (Offline Mode)**
```cpp
bool enableE2FileLogging = false;  // true = file logging, false = RIC connection
Config::SetDefault("ns3::LteEnbNetDevice::EnableE2FileLogging", BooleanValue(enableE2FileLogging));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableE2FileLogging", BooleanValue(enableE2FileLogging));

// Both RIC connection and file logging
bool e2andLogging = false;
Config::SetDefault("ns3::LteEnbNetDevice::e2andLogging", BooleanValue(e2andLogging));
Config::SetDefault("ns3::MmWaveEnbNetDevice::e2andLogging", BooleanValue(e2andLogging));
```

#### **E2 Control File (External Control)**
```cpp
std::string controlFilename = "";  // Path to control file (optional)
Config::SetDefault("ns3::LteEnbNetDevice::ControlFileName", StringValue(controlFilename));
```

#### **Reduced PM Values (Subset of Metrics)**
```cpp
bool reducedPmValues = false;  // true = subset of metrics, false = all metrics
Config::SetDefault("ns3::MmWaveEnbNetDevice::ReducedPmValues", BooleanValue(reducedPmValues));
Config::SetDefault("ns3::LteEnbNetDevice::ReducedPmValues", BooleanValue(reducedPmValues));
```

### 5.2 KPM Metrics Collected

#### **DU Metrics (PHY, MAC, RLC)**
- **PHY Metrics**: SINR, RSRP, RSRQ, throughput
- **MAC Metrics**: Buffer status, scheduling information
- **RLC Metrics**: PDU statistics, buffer occupancy

#### **CU-UP Metrics (PDCP)**
- **PDCP Metrics**: Throughput, latency, packet loss

#### **CU-CP Metrics (RRC)**
- **RRC Metrics**: Connection status, handover events

### 5.3 E2 Report Configuration Summary

| Configuration | Description | Default |
|--------------|-------------|---------|
| `indicationPeriodicity` | Report interval (seconds) | 0.1 (100 ms) |
| `e2lteEnabled` | Enable LTE E2 reports | true |
| `e2nrEnabled` | Enable NR E2 reports | true |
| `e2du` | Enable DU reports | true |
| `e2cuUp` | Enable CU-UP reports | true |
| `e2cuCp` | Enable CU-CP reports | true |
| `e2TermIp` | RIC IP address | "127.0.0.1" |
| `enableE2FileLogging` | File logging mode | false |
| `g_e2_func_id` | KPM function ID | 2 |
| `g_rc_e2_func_id` | RC function ID | 3 |
| `reducedPmValues` | Subset of metrics | false |

---

## 6. Step-by-Step Configuration Guide

### Step 1: Include Headers
```cpp
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mmwave-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "../src/mmwave/model/node-container-manager.h"
#include "ns3/lte-helper.h"
#include "ns3/isotropic-antenna-model.h"
```

### Step 2: Configure Physical Layer
```cpp
// Frequency and bandwidth
double bandwidth = 20e6;        // 20 MHz
double centerFrequency = 3.5e9;  // 3.5 GHz

Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bandwidth));
Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(centerFrequency));
Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency", DoubleValue(centerFrequency));
Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue(false));

// Antenna
Config::SetDefault("ns3::PhasedArrayModel::AntennaElement", 
    PointerValue(CreateObject<IsotropicAntennaModel>()));
Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(1));
Config::SetDefault("ns3::McUeNetDevice::AntennaNum", UintegerValue(1));

// Channel model update period
Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(100.0)));
Config::SetDefault("ns3::ThreeGppChannelConditionModel::UpdatePeriod", TimeValue(MilliSeconds(100)));
```

### Step 3: Configure MAC/RLC Layer
```cpp
// HARQ
bool harqEnabled = true;
Config::SetDefault("ns3::MmWaveHelper::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));

// RLC buffer
uint32_t bufferSize = 10;  // MB
Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MilliSeconds(10.0)));
Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));

// Random access
uint8_t numberOfRaPreambles = 40;
Config::SetDefault("ns3::MmWaveEnbMac::NumberOfRaPreambles", UintegerValue(numberOfRaPreambles));

// RRC
Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));
```

### Step 4: Configure Handover
```cpp
std::string handoverMode = "DynamicTtt";  // "NoAuto", "FixedTtt", "DynamicTtt", "Threshold"
double hoSinrDifference = 3.0;  // dB
double outageThreshold = -5.0;  // dB

Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue(handoverMode));
Config::SetDefault("ns3::LteEnbRrc::HoSinrDifference", DoubleValue(hoSinrDifference));
Config::SetDefault("ns3::LteEnbRrc::OutageThreshold", DoubleValue(outageThreshold));
```

### Step 5: Configure E2 Interface
```cpp
// E2 periodicity
double indicationPeriodicity = 0.1;  // seconds
Config::SetDefault("ns3::LteEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));

// E2 mode
bool e2lteEnabled = true;   // For EN-DC
bool e2nrEnabled = true;    // For SA or EN-DC
Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));
Config::SetDefault("ns3::MmWaveHelper::E2ModeNr", BooleanValue(e2nrEnabled));

// E2 reports
bool e2du = true;
bool e2cuUp = true;
bool e2cuCp = true;
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(e2du));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));

// E2 function IDs
double g_e2_func_id = 2;      // KPM
double g_rc_e2_func_id = 3;   // RC
Config::SetDefault("ns3::MmWaveEnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));
Config::SetDefault("ns3::MmWaveEnbNetDevice::RC_E2functionID", DoubleValue(g_rc_e2_func_id));

// E2 termination IP
std::string e2TermIp = "127.0.0.1";
Config::SetDefault("ns3::MmWaveHelper::E2TermIp", StringValue(e2TermIp));

// E2 file logging
bool enableE2FileLogging = false;
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableE2FileLogging", BooleanValue(enableE2FileLogging));
```

### Step 6: Create mmWave Helper
```cpp
Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");

Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
mmwaveHelper->SetEpcHelper(epcHelper);
```

### Step 7: Setup Core Network (EPC)
```cpp
// Get PGW
Ptr<Node> pgw = epcHelper->GetPgwNode();

// Create remote host
NodeContainer remoteHostContainer;
remoteHostContainer.Create(1);
Ptr<Node> remoteHost = remoteHostContainer.Get(0);
InternetStackHelper internet;
internet.Install(remoteHostContainer);

// Connect PGW to remote host
PointToPointHelper p2ph;
p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

// Assign IP addresses
Ipv4AddressHelper ipv4h;
ipv4h.SetBase("1.0.0.0", "255.0.0.0");
Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

// Configure routing
Ipv4StaticRoutingHelper ipv4RoutingHelper;
Ptr<Ipv4StaticRouting> remoteHostStaticRouting = 
    ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
```

### Step 8: Create Nodes
```cpp
// Node counts
uint8_t nMmWaveEnbNodes = 2;
uint8_t nLteEnbNodes = 0;   // 0 for SA, 1+ for EN-DC
uint32_t nUeNodes = 20;

// Create node containers
NodeContainer ueNodes;
NodeContainer mmWaveEnbNodes;
NodeContainer lteEnbNodes;
NodeContainer allEnbNodes;

mmWaveEnbNodes.Create(nMmWaveEnbNodes);
if (nLteEnbNodes > 0) {
    lteEnbNodes.Create(nLteEnbNodes);
}
ueNodes.Create(nUeNodes);

allEnbNodes.Add(lteEnbNodes);
allEnbNodes.Add(mmWaveEnbNodes);

// Set node container manager (required for E2)
NodeContainerManager::GetInstance().SetMmWaveEnbNodes(mmWaveEnbNodes);
```

### Step 9: Configure Node Positions
```cpp
// Scenario dimensions
double maxXAxis = 4000;
double maxYAxis = 4000;
Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);

// eNB positions
Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
if (nLteEnbNodes > 0) {
    enbPositionAlloc->Add(centerPosition);  // LTE eNB at center
}
enbPositionAlloc->Add(centerPosition);      // First mmWave gNB at center

// Remaining mmWave gNBs in circular constellation
double isd_cell = 500;
double nConstellation = nMmWaveEnbNodes - 1;
for (int8_t i = 0; i < nConstellation; ++i) {
    double x = isd_cell * cos((2 * M_PI * i) / (nConstellation));
    double y = isd_cell * sin((2 * M_PI * i) / (nConstellation));
    enbPositionAlloc->Add(Vector(centerPosition.x + x, centerPosition.y + y, 3));
}

MobilityHelper enbmobility;
enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
enbmobility.SetPositionAllocator(enbPositionAlloc);
enbmobility.Install(allEnbNodes);

// UE positions
double isd_ue = 1000;
Ptr<UniformDiscPositionAllocator> uePositionAlloc = CreateObject<UniformDiscPositionAllocator>();
uePositionAlloc->SetX(centerPosition.x);
uePositionAlloc->SetY(centerPosition.y);
uePositionAlloc->SetRho(isd_ue);

Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
speed->SetAttribute("Min", DoubleValue(2.0));
speed->SetAttribute("Max", DoubleValue(4.0));

MobilityHelper uemobility;
uemobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel", 
    "Speed", PointerValue(speed),
    "Bounds", RectangleValue(Rectangle(0, maxXAxis, 0, maxYAxis)));
uemobility.SetPositionAllocator(uePositionAlloc);
uemobility.Install(ueNodes);
```

### Step 10: Install Devices
```cpp
// Install devices
NetDeviceContainer lteEnbDevs;
NetDeviceContainer mmWaveEnbDevs;
NetDeviceContainer ueDevs;

if (nLteEnbNodes > 0) {
    lteEnbDevs = mmwaveHelper->InstallLteEnbDevice(lteEnbNodes);
}
mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);

// For SA: use mmWave-only UEs
// For EN-DC: use multi-connectivity UEs
if (nLteEnbNodes > 0) {
    ueDevs = mmwaveHelper->InstallMcUeDevice(ueNodes);  // EN-DC
} else {
    ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);    // SA
}

// Install IP stack on UEs
internet.Install(ueNodes);
Ipv4InterfaceContainer ueIpIface;
ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

// Configure UE default gateway
for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
    Ptr<Node> ueNode = ueNodes.Get(u);
    Ptr<Ipv4StaticRouting> ueStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
}
```

### Step 11: Setup X2 Interface (EN-DC only)
```cpp
if (nLteEnbNodes > 0) {
    mmwaveHelper->AddX2Interface(lteEnbNodes, mmWaveEnbNodes);  // Required for EN-DC
}
```

### Step 12: Attach UEs to eNBs
```cpp
if (nLteEnbNodes > 0) {
    mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs, lteEnbDevs);  // EN-DC
} else {
    mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs);  // SA
}
```

### Step 13: Install Applications
```cpp
// UDP sink on remote host
uint16_t portUdp = 60000;
Address sinkLocalAddressUdp(InetSocketAddress(Ipv4Address::GetAny(), portUdp));
PacketSinkHelper sinkHelperUdp("ns3::UdpSocketFactory", sinkLocalAddressUdp);
ApplicationContainer sinkApp;
sinkApp.Add(sinkHelperUdp.Install(remoteHost));

// UDP client applications (downlink from remote host to UEs)
ApplicationContainer clientApp;
for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), 1234));
    sinkApp.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));
    
    UdpClientHelper dlClient(ueIpIface.GetAddress(u), 1234);
    dlClient.SetAttribute("Interval", TimeValue(MicroSeconds(500)));  // 2 Mbps
    dlClient.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
    dlClient.SetAttribute("PacketSize", UintegerValue(200));  // 200 bytes
    clientApp.Add(dlClient.Install(remoteHost));
}

// Start applications
sinkApp.Start(Seconds(0));
clientApp.Start(MilliSeconds(100));
clientApp.Stop(Seconds(simTime - 0.1));
```

### Step 14: Enable Traces (Optional)
```cpp
bool enableTraces = true;
if (enableTraces) {
    mmwaveHelper->EnableTraces();
    
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->Initialize();
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
}
```

### Step 15: Run Simulation
```cpp
double simTime = 1000;  // seconds
Simulator::Stop(Seconds(simTime));
Simulator::Run();
Simulator::Destroy();
```

---

## 7. Example: 2-Cell 5G SA Setup with 20 UEs

### Complete Configuration
```cpp
// Step 1: Physical layer configuration
double bandwidth = 20e6;        // 20 MHz
double centerFrequency = 3.5e9;  // 3.5 GHz
Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bandwidth));
Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(centerFrequency));
Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency", DoubleValue(centerFrequency));
Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue(false));
Config::SetDefault("ns3::PhasedArrayModel::AntennaElement", 
    PointerValue(CreateObject<IsotropicAntennaModel>()));
Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(1));

// Step 2: MAC/RLC configuration
bool harqEnabled = true;
Config::SetDefault("ns3::MmWaveHelper::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));
uint32_t bufferSize = 10;  // MB
Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));

// Step 3: Handover configuration
std::string handoverMode = "DynamicTtt";
double hoSinrDifference = 3.0;  // dB
Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue(handoverMode));
Config::SetDefault("ns3::LteEnbRrc::HoSinrDifference", DoubleValue(hoSinrDifference));

// Step 4: E2 interface configuration
double indicationPeriodicity = 0.1;  // 100 ms
bool e2lteEnabled = false;   // SA: no LTE
bool e2nrEnabled = true;     // SA: NR only
bool e2du = true;
bool e2cuUp = true;
bool e2cuCp = true;
double g_e2_func_id = 2;      // KPM
std::string e2TermIp = "127.0.0.1";
bool enableE2FileLogging = false;

Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));
Config::SetDefault("ns3::MmWaveHelper::E2ModeNr", BooleanValue(e2nrEnabled));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(e2du));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));
Config::SetDefault("ns3::MmWaveEnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));
Config::SetDefault("ns3::MmWaveHelper::E2TermIp", StringValue(e2TermIp));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableE2FileLogging", BooleanValue(enableE2FileLogging));

// Step 5: Create mmWave helper
Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
mmwaveHelper->SetEpcHelper(epcHelper);

// Step 6: Setup core network (EPC)
Ptr<Node> pgw = epcHelper->GetPgwNode();
NodeContainer remoteHostContainer;
remoteHostContainer.Create(1);
Ptr<Node> remoteHost = remoteHostContainer.Get(0);
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
Ptr<Ipv4StaticRouting> remoteHostStaticRouting = 
    ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

// Step 7: Create nodes
uint8_t nMmWaveEnbNodes = 2;   // 2 gNBs
uint8_t nLteEnbNodes = 0;      // SA: no LTE eNBs
uint32_t nUeNodes = 20;        // 20 UEs

NodeContainer ueNodes;
NodeContainer mmWaveEnbNodes;
NodeContainer allEnbNodes;

mmWaveEnbNodes.Create(nMmWaveEnbNodes);
ueNodes.Create(nUeNodes);
allEnbNodes.Add(mmWaveEnbNodes);

NodeContainerManager::GetInstance().SetMmWaveEnbNodes(mmWaveEnbNodes);

// Step 8: Configure node positions
double maxXAxis = 4000;
double maxYAxis = 4000;
Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);

Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
enbPositionAlloc->Add(centerPosition);  // First gNB at center

double isd_cell = 500;
double nConstellation = nMmWaveEnbNodes - 1;
for (int8_t i = 0; i < nConstellation; ++i) {
    double x = isd_cell * cos((2 * M_PI * i) / (nConstellation));
    double y = isd_cell * sin((2 * M_PI * i) / (nConstellation));
    enbPositionAlloc->Add(Vector(centerPosition.x + x, centerPosition.y + y, 3));
}

MobilityHelper enbmobility;
enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
enbmobility.SetPositionAllocator(enbPositionAlloc);
enbmobility.Install(allEnbNodes);

double isd_ue = 1000;
Ptr<UniformDiscPositionAllocator> uePositionAlloc = CreateObject<UniformDiscPositionAllocator>();
uePositionAlloc->SetX(centerPosition.x);
uePositionAlloc->SetY(centerPosition.y);
uePositionAlloc->SetRho(isd_ue);

Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
speed->SetAttribute("Min", DoubleValue(2.0));
speed->SetAttribute("Max", DoubleValue(4.0));

MobilityHelper uemobility;
uemobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel", 
    "Speed", PointerValue(speed),
    "Bounds", RectangleValue(Rectangle(0, maxXAxis, 0, maxYAxis)));
uemobility.SetPositionAllocator(uePositionAlloc);
uemobility.Install(ueNodes);

// Step 9: Install devices
NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);
NetDeviceContainer mmWaveUeDevs = mmwaveHelper->InstallUeDevice(ueNodes);  // SA: mmWave-only UEs

internet.Install(ueNodes);
Ipv4InterfaceContainer ueIpIface;
ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(mmWaveUeDevs));

for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
    Ptr<Node> ueNode = ueNodes.Get(u);
    Ptr<Ipv4StaticRouting> ueStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
}

// Step 10: Attach UEs to gNBs
mmwaveHelper->AttachToClosestEnb(mmWaveUeDevs, mmWaveEnbDevs);  // SA: no LTE

// Step 11: Install applications
uint16_t portUdp = 60000;
Address sinkLocalAddressUdp(InetSocketAddress(Ipv4Address::GetAny(), portUdp));
PacketSinkHelper sinkHelperUdp("ns3::UdpSocketFactory", sinkLocalAddressUdp);
ApplicationContainer sinkApp;
sinkApp.Add(sinkHelperUdp.Install(remoteHost));

ApplicationContainer clientApp;
for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), 1234));
    sinkApp.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));
    
    UdpClientHelper dlClient(ueIpIface.GetAddress(u), 1234);
    dlClient.SetAttribute("Interval", TimeValue(MicroSeconds(500)));
    dlClient.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
    dlClient.SetAttribute("PacketSize", UintegerValue(200));
    clientApp.Add(dlClient.Install(remoteHost));
}

sinkApp.Start(Seconds(0));
clientApp.Start(MilliSeconds(100));

// Step 12: Run simulation
double simTime = 1000;
clientApp.Stop(Seconds(simTime - 0.1));
Simulator::Stop(Seconds(simTime));
Simulator::Run();
Simulator::Destroy();
```

---

## 8. Configuration Parameters Reference

### 8.1 Scenario Type Selection

| Parameter | SA (Standalone) | EN-DC (Dual Connectivity) |
|-----------|-----------------|---------------------------|
| `nLteEnbNodes` | 0 | 1+ |
| `nMmWaveEnbNodes` | 2+ | 1+ |
| `UE Device Type` | `InstallUeDevice()` | `InstallMcUeDevice()` |
| `e2lteEnabled` | false | true |
| `e2nrEnabled` | true | true |
| `X2 Interface` | Optional | Required |

### 8.2 Physical Layer Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `bandwidth` | 20e6 Hz (20 MHz) | Carrier bandwidth |
| `centerFrequency` | 3.5e9 Hz (3.5 GHz) | Center frequency |
| `AntennaNum` | 1 | Number of antennas |
| `PathlossModel` | ThreeGppUmiStreetCanyon | Propagation model |
| `ChannelConditionModel` | ThreeGppUmiStreetCanyon | Channel condition model |
| `ShadowingEnabled` | false | Enable shadowing |

### 8.3 MAC/RLC Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `HarqEnabled` | true | Enable HARQ |
| `NumHarqProcess` | 100 | Number of HARQ processes |
| `MaxTxBufferSize` | 10 MB | RLC buffer size |
| `NumberOfRaPreambles` | 40 | Random access preambles |
| `UseIdealRrc` | true | Ideal RRC (no errors) |

### 8.4 Handover Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `handoverMode` | "DynamicTtt" | Handover algorithm |
| `hoSinrDifference` | 3.0 dB | Handover SINR difference |
| `outageThreshold` | -5.0 dB | Outage SNR threshold |

### 8.5 E2 Interface Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `indicationPeriodicity` | 0.1 s (100 ms) | Report interval |
| `e2lteEnabled` | true | Enable LTE E2 reports |
| `e2nrEnabled` | true | Enable NR E2 reports |
| `e2du` | true | Enable DU reports |
| `e2cuUp` | true | Enable CU-UP reports |
| `e2cuCp` | true | Enable CU-CP reports |
| `e2TermIp` | "127.0.0.1" | RIC IP address |
| `enableE2FileLogging` | false | File logging mode |
| `g_e2_func_id` | 2 | KPM function ID |
| `g_rc_e2_func_id` | 3 | RC function ID |
| `reducedPmValues` | false | Subset of metrics |

### 8.6 Network Topology Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `maxXAxis` | 4000 m | Simulation area width |
| `maxYAxis` | 4000 m | Simulation area height |
| `isd_cell` | 500 m | Inter-site distance for cells |
| `isd_ue` | 1000 m | Radius for UE distribution |
| `nMmWaveEnbNodes` | 2+ | Number of mmWave gNBs |
| `nLteEnbNodes` | 0 or 1+ | Number of LTE eNBs |
| `nUeNodes` | 20 | Number of UEs |

### 8.7 Application Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `PacketSize` | 200 bytes | UDP packet size |
| `Interval` | 500 μs | Packet interval (2 Mbps) |
| `StartTime` | 0.1 s | Application start time |
| `simTime` | 1000 s | Simulation time |

---

## 9. KPM Metrics Collection

### 9.1 Metrics Available via E2 Interface

#### **DU Metrics (PHY, MAC, RLC)**
- **PHY**: SINR, RSRP, RSRQ, throughput, BLER
- **MAC**: Buffer status, scheduling information, HARQ statistics
- **RLC**: PDU statistics, buffer occupancy, retransmissions

#### **CU-UP Metrics (PDCP)**
- **PDCP**: Throughput, latency, packet loss, PDU statistics

#### **CU-CP Metrics (RRC)**
- **RRC**: Connection status, handover events, cell selection/reselection

### 9.2 Accessing KPM Metrics

#### **Via RIC (RAN Intelligent Controller)**
- Set `enableE2FileLogging = false`
- Set `e2TermIp` to RIC IP address
- Metrics are sent via E2 interface to RIC

#### **Via File Logging**
- Set `enableE2FileLogging = true`
- Metrics are logged to files (location depends on implementation)

### 9.3 Metrics Collection Configuration
```cpp
// Enable all E2 reports
bool e2du = true;     // DU reports (PHY, MAC, RLC)
bool e2cuUp = true;   // CU-UP reports (PDCP)
bool e2cuCp = true;   // CU-CP reports (RRC)

// Set report periodicity
double indicationPeriodicity = 0.1;  // 100 ms

// Configure E2 interface
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(e2du));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));
Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
```

---

## 10. Common Modifications

### 10.1 Change Scenario Type (SA ↔ EN-DC)

**From SA to EN-DC:**
```cpp
// Change node counts
uint8_t nLteEnbNodes = 1;        // Add LTE eNB
uint8_t nMmWaveEnbNodes = 2;     // Keep mmWave gNBs

// Change UE device type
NetDeviceContainer ueDevs = mmwaveHelper->InstallMcUeDevice(ueNodes);  // Multi-connectivity

// Enable LTE E2 reports
bool e2lteEnabled = true;
Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));

// Add X2 interface
mmwaveHelper->AddX2Interface(lteEnbNodes, mmWaveEnbNodes);

// Attach UEs
mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs, lteEnbDevs);
```

**From EN-DC to SA:**
```cpp
// Change node counts
uint8_t nLteEnbNodes = 0;        // Remove LTE eNB
uint8_t nMmWaveEnbNodes = 2;     // Keep mmWave gNBs

// Change UE device type
NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);  // mmWave-only

// Disable LTE E2 reports
bool e2lteEnabled = false;
Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));

// Remove X2 interface (not needed for SA)

// Attach UEs
mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs);
```

### 10.2 Change Number of Cells and UEs

```cpp
// Change node counts
uint8_t nMmWaveEnbNodes = 4;     // 4 gNBs
uint8_t nLteEnbNodes = 1;        // 1 LTE eNB (for EN-DC)
uint32_t nUeNodes = 50;          // 50 UEs

// Update positioning (circular constellation adjusts automatically)
double nConstellation = nMmWaveEnbNodes - 1;
for (int8_t i = 0; i < nConstellation; ++i) {
    // Positions calculated automatically
}
```

### 10.3 Change Frequency and Bandwidth

```cpp
// Change frequency
double centerFrequency = 28e9;   // 28 GHz (mmWave)
double bandwidth = 100e6;        // 100 MHz

Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bandwidth));
Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(centerFrequency));
Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency", DoubleValue(centerFrequency));
```

### 10.4 Change Mobility Model

```cpp
// Change to constant velocity
MobilityHelper uemobility;
uemobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
uemobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel",
    "Velocity", VectorValue(Vector(10, 0, 0)));  // 10 m/s in X direction
uemobility.SetPositionAllocator(uePositionAlloc);
uemobility.Install(ueNodes);
```

### 10.5 Change Pathloss Model

```cpp
// Change to rural macro
mmwaveHelper->SetPathlossModelType("ns3::ThreeGppRmaPropagationLossModel");
mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppRmaChannelConditionModel");
```

### 10.6 Change E2 Report Periodicity

```cpp
// Change to 1 second
double indicationPeriodicity = 1.0;  // 1 second
Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
```

### 10.7 Change Application Traffic

```cpp
// Change to TCP
BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(ueIpIface.GetAddress(u), 1234));
source.SetAttribute("MaxBytes", UintegerValue(0));
ApplicationContainer sourceApp = source.Install(remoteHost);
sourceApp.Start(Seconds(0.1));
```

---

## 11. Troubleshooting

### 11.1 Common Issues

#### **E2 Interface Not Working**
- Check `e2TermIp` is correct
- Check `enableE2FileLogging` setting
- Verify `NodeContainerManager::GetInstance().SetMmWaveEnbNodes()` is called
- Check E2 reports are enabled (`e2du`, `e2cuUp`, `e2cuCp`)

#### **UEs Not Attaching**
- Check device types match scenario (SA vs EN-DC)
- Verify X2 interface is set up for EN-DC
- Check node positions are valid
- Verify EPC is set up correctly

#### **No Traffic**
- Check applications are installed and started
- Verify IP addresses are assigned
- Check routing is configured
- Verify UEs are attached to eNBs

#### **Handover Not Working**
- Check handover mode is set correctly
- Verify SINR difference threshold
- Check X2 interface is set up (for EN-DC)
- Verify handover algorithm is enabled

### 11.2 Debugging Tips

#### **Enable Logging**
```cpp
LogComponentEnableAll(LOG_PREFIX_ALL);
LogComponentEnable("MmWaveEnbNetDevice", LOG_LEVEL_INFO);
LogComponentEnable("LteEnbNetDevice", LOG_LEVEL_INFO);
LogComponentEnable("E2Termination", LOG_LEVEL_DEBUG);
```

#### **Check Node Positions**
```cpp
for (uint32_t i = 0; i < mmWaveEnbNodes.GetN(); ++i) {
    Ptr<Node> node = mmWaveEnbNodes.Get(i);
    Vector pos = node->GetObject<MobilityModel>()->GetPosition();
    NS_LOG_UNCOND("gNB " << i << " position: " << pos);
}
```

#### **Check UE Attachment**
```cpp
for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
    Ptr<Node> ueNode = ueNodes.Get(i);
    Ptr<NetDevice> dev = ueNode->GetDevice(0);
    // Check device type and attachment
}
```

---

## 12. Summary

### Key Takeaways

1. **Scenario Type Selection**: Choose SA (standalone) or EN-DC (dual connectivity) based on requirements
2. **Device Types**: Use `InstallUeDevice()` for SA, `InstallMcUeDevice()` for EN-DC
3. **E2 Interface**: Configure E2 reports (DU, CU-UP, CU-CP) to collect KPM metrics
4. **Network Topology**: Configure node counts, positions, and mobility
5. **Physical Layer**: Configure frequency, bandwidth, pathloss, and channel models
6. **Applications**: Install traffic sources and sinks
7. **KPM Metrics**: Access metrics via RIC or file logging

### Configuration Checklist

- [ ] Include required headers
- [ ] Configure physical layer (frequency, bandwidth, pathloss)
- [ ] Configure MAC/RLC (HARQ, buffer, RRC)
- [ ] Configure handover (mode, thresholds)
- [ ] Configure E2 interface (periodicity, reports, RIC IP)
- [ ] Create mmWave helper and EPC helper
- [ ] Setup core network (PGW, remote host, routing)
- [ ] Create nodes (eNBs, UEs)
- [ ] Configure node positions and mobility
- [ ] Install devices (eNBs, UEs)
- [ ] Setup X2 interface (EN-DC only)
- [ ] Attach UEs to eNBs
- [ ] Install applications
- [ ] Enable traces (optional)
- [ ] Run simulation

---

**End of NS-3 Component Configuration Guide**

This guide provides all the information needed to create custom NS-3 scenarios and collect KPM metrics via O-RAN E2 interface. Modify the parameters as needed for your specific scenario requirements.

