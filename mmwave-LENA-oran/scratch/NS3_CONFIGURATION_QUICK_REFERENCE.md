# NS-3 Configuration Quick Reference Card

## 🎯 Scenario Type Selection

### Standalone (SA) 5G
```cpp
uint8_t nLteEnbNodes = 0;        // No LTE eNBs
uint8_t nMmWaveEnbNodes = 2;     // 2+ mmWave gNBs
NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);  // mmWave-only UEs
bool e2lteEnabled = false;       // No LTE E2 reports
bool e2nrEnabled = true;         // NR E2 reports only
// NO X2 interface needed
mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs);  // Attach to gNBs only
```

### EN-DC (Dual Connectivity)
```cpp
uint8_t nLteEnbNodes = 1;        // 1+ LTE eNBs (anchor)
uint8_t nMmWaveEnbNodes = 2;     // 1+ mmWave gNBs (secondary)
NetDeviceContainer ueDevs = mmwaveHelper->InstallMcUeDevice(ueNodes);  // Multi-connectivity UEs
bool e2lteEnabled = true;        // LTE E2 reports
bool e2nrEnabled = true;         // NR E2 reports
mmwaveHelper->AddX2Interface(lteEnbNodes, mmWaveEnbNodes);  // X2 interface REQUIRED
mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs, lteEnbDevs);  // Attach to LTE + mmWave
```

---

## 📡 Physical Layer Configuration

### Frequency & Bandwidth
```cpp
double bandwidth = 20e6;        // 20 MHz
double centerFrequency = 3.5e9;  // 3.5 GHz (or 28e9 for mmWave)
Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bandwidth));
Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(centerFrequency));
Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency", DoubleValue(centerFrequency));
```

### Pathloss Model
```cpp
mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
// Options: ThreeGppUmiStreetCanyon, ThreeGppRma, ThreeGppUma, FriisPropagationLossModel
```

### Channel Model
```cpp
mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
// Options: ThreeGppUmiStreetCanyon, ThreeGppRma, ThreeGppUma
```

### Antenna
```cpp
Config::SetDefault("ns3::PhasedArrayModel::AntennaElement", 
    PointerValue(CreateObject<IsotropicAntennaModel>()));
Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(1));
```

---

## 🔧 MAC/RLC Configuration

### HARQ
```cpp
bool harqEnabled = true;
Config::SetDefault("ns3::MmWaveHelper::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(harqEnabled));
Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));
```

### RLC Buffer
```cpp
uint32_t bufferSize = 10;  // MB
Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
```

### Random Access
```cpp
uint8_t numberOfRaPreambles = 40;
Config::SetDefault("ns3::MmWaveEnbMac::NumberOfRaPreambles", UintegerValue(numberOfRaPreambles));
```

### RRC
```cpp
Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));  // Ideal RRC (no errors)
```

---

## 🔀 Handover Configuration

```cpp
std::string handoverMode = "DynamicTtt";  // Options: "NoAuto", "FixedTtt", "DynamicTtt", "Threshold"
double hoSinrDifference = 3.0;  // dB
double outageThreshold = -5.0;  // dB

Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue(handoverMode));
Config::SetDefault("ns3::LteEnbRrc::HoSinrDifference", DoubleValue(hoSinrDifference));
Config::SetDefault("ns3::LteEnbRrc::OutageThreshold", DoubleValue(outageThreshold));
```

---

## 📊 E2 Interface & KPM Metrics Configuration

### E2 Periodicity (Report Interval)
```cpp
double indicationPeriodicity = 0.1;  // seconds (100 ms)
Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
Config::SetDefault("ns3::LteEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
```

### E2 Mode (LTE vs NR)
```cpp
bool e2lteEnabled = true;   // Enable LTE E2 reports (for EN-DC)
bool e2nrEnabled = true;    // Enable NR E2 reports (for SA or EN-DC)
Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));
Config::SetDefault("ns3::MmWaveHelper::E2ModeNr", BooleanValue(e2nrEnabled));
```

### E2 Reports (DU, CU-UP, CU-CP)
```cpp
bool e2du = true;     // DU reports (PHY, MAC, RLC)
bool e2cuUp = true;   // CU-UP reports (PDCP)
bool e2cuCp = true;   // CU-CP reports (RRC)

Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(e2du));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));
```

### E2 Function IDs
```cpp
double g_e2_func_id = 2;      // KPM (Key Performance Metrics) function ID
double g_rc_e2_func_id = 3;   // RC (RAN Control) function ID

Config::SetDefault("ns3::MmWaveEnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));
Config::SetDefault("ns3::MmWaveEnbNetDevice::RC_E2functionID", DoubleValue(g_rc_e2_func_id));
```

### E2 Termination IP (RIC Connection)
```cpp
std::string e2TermIp = "127.0.0.1";  // RIC IP address
Config::SetDefault("ns3::MmWaveHelper::E2TermIp", StringValue(e2TermIp));
```

### E2 File Logging (Offline Mode)
```cpp
bool enableE2FileLogging = false;  // true = file logging, false = RIC connection
Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableE2FileLogging", BooleanValue(enableE2FileLogging));
```

### Reduced PM Values
```cpp
bool reducedPmValues = false;  // true = subset of metrics, false = all metrics
Config::SetDefault("ns3::MmWaveEnbNetDevice::ReducedPmValues", BooleanValue(reducedPmValues));
```

---

## 🌐 Network Topology Configuration

### Node Counts
```cpp
uint8_t nMmWaveEnbNodes = 2;    // Number of mmWave gNBs
uint8_t nLteEnbNodes = 0;       // Number of LTE eNBs (0 for SA, 1+ for EN-DC)
uint32_t nUeNodes = 20;         // Number of UEs
```

### Scenario Dimensions
```cpp
double maxXAxis = 4000;  // meters (simulation area width)
double maxYAxis = 4000;  // meters (simulation area height)
```

### Inter-Site Distance
```cpp
double isd_cell = 500;   // Inter-site distance for cells (meters)
double isd_ue = 1000;    // Radius for UE distribution (meters)
```

### Node Positioning
```cpp
Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);

// eNB positions (circular constellation)
Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
enbPositionAlloc->Add(centerPosition);  // First eNB at center

double nConstellation = nMmWaveEnbNodes - 1;
for (int8_t i = 0; i < nConstellation; ++i) {
    double x = isd_cell * cos((2 * M_PI * i) / (nConstellation));
    double y = isd_cell * sin((2 * M_PI * i) / (nConstellation));
    enbPositionAlloc->Add(Vector(centerPosition.x + x, centerPosition.y + y, 3));
}

// UE positions (uniform disc)
Ptr<UniformDiscPositionAllocator> uePositionAlloc = CreateObject<UniformDiscPositionAllocator>();
uePositionAlloc->SetX(centerPosition.x);
uePositionAlloc->SetY(centerPosition.y);
uePositionAlloc->SetRho(isd_ue);
```

### Mobility Model
```cpp
// eNB mobility (fixed)
MobilityHelper enbmobility;
enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
enbmobility.SetPositionAllocator(enbPositionAlloc);
enbmobility.Install(allEnbNodes);

// UE mobility (random walk)
Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
speed->SetAttribute("Min", DoubleValue(2.0));  // 2 m/s
speed->SetAttribute("Max", DoubleValue(4.0));  // 4 m/s

MobilityHelper uemobility;
uemobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel", 
    "Speed", PointerValue(speed),
    "Bounds", RectangleValue(Rectangle(0, maxXAxis, 0, maxYAxis)));
uemobility.SetPositionAllocator(uePositionAlloc);
uemobility.Install(ueNodes);
```

---

## 🚀 Core Network (EPC) Setup

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

---

## 📱 Device Installation

### Create mmWave Helper
```cpp
Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");

Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
mmwaveHelper->SetEpcHelper(epcHelper);
```

### Install Devices
```cpp
// Install eNB devices
NetDeviceContainer lteEnbDevs;
NetDeviceContainer mmWaveEnbDevs;

if (nLteEnbNodes > 0) {
    lteEnbDevs = mmwaveHelper->InstallLteEnbDevice(lteEnbNodes);
}
mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);

// Install UE devices
NetDeviceContainer ueDevs;
if (nLteEnbNodes > 0) {
    ueDevs = mmwaveHelper->InstallMcUeDevice(ueNodes);  // EN-DC: Multi-connectivity
} else {
    ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);    // SA: mmWave-only
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

### X2 Interface (EN-DC only)
```cpp
if (nLteEnbNodes > 0) {
    mmwaveHelper->AddX2Interface(lteEnbNodes, mmWaveEnbNodes);  // REQUIRED for EN-DC
}
```

### Attach UEs to eNBs
```cpp
if (nLteEnbNodes > 0) {
    mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs, lteEnbDevs);  // EN-DC
} else {
    mmwaveHelper->AttachToClosestEnb(ueDevs, mmWaveEnbDevs);  // SA
}
```

### Node Container Manager (Required for E2)
```cpp
NodeContainerManager::GetInstance().SetMmWaveEnbNodes(mmWaveEnbNodes);
```

---

## 📶 Application Setup

### UDP Sink Application (Remote Host)
```cpp
uint16_t portUdp = 60000;
Address sinkLocalAddressUdp(InetSocketAddress(Ipv4Address::GetAny(), portUdp));
PacketSinkHelper sinkHelperUdp("ns3::UdpSocketFactory", sinkLocalAddressUdp);
ApplicationContainer sinkApp;
sinkApp.Add(sinkHelperUdp.Install(remoteHost));
```

### UDP Client Application (UEs)
```cpp
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

---

## 🎬 Simulation Execution

```cpp
double simTime = 1000;  // seconds
Simulator::Stop(Seconds(simTime));
Simulator::Run();
Simulator::Destroy();
```

---

## 📋 Configuration Parameters Summary

| Category | Parameter | Default | Description |
|----------|-----------|---------|-------------|
| **Scenario** | `nMmWaveEnbNodes` | 2+ | Number of mmWave gNBs |
| | `nLteEnbNodes` | 0 or 1+ | Number of LTE eNBs (0 for SA, 1+ for EN-DC) |
| | `nUeNodes` | 20 | Number of UEs |
| **Physical** | `bandwidth` | 20e6 Hz (20 MHz) | Carrier bandwidth |
| | `centerFrequency` | 3.5e9 Hz (3.5 GHz) | Center frequency |
| | `AntennaNum` | 1 | Number of antennas |
| **MAC/RLC** | `HarqEnabled` | true | Enable HARQ |
| | `MaxTxBufferSize` | 10 MB | RLC buffer size |
| | `NumberOfRaPreambles` | 40 | Random access preambles |
| **Handover** | `handoverMode` | "DynamicTtt" | Handover algorithm |
| | `hoSinrDifference` | 3.0 dB | Handover SINR difference |
| | `outageThreshold` | -5.0 dB | Outage SNR threshold |
| **E2 Interface** | `indicationPeriodicity` | 0.1 s (100 ms) | Report interval |
| | `e2lteEnabled` | true/false | Enable LTE E2 reports |
| | `e2nrEnabled` | true | Enable NR E2 reports |
| | `e2du` | true | Enable DU reports |
| | `e2cuUp` | true | Enable CU-UP reports |
| | `e2cuCp` | true | Enable CU-CP reports |
| | `e2TermIp` | "127.0.0.1" | RIC IP address |
| | `enableE2FileLogging` | false | File logging mode |
| | `g_e2_func_id` | 2 | KPM function ID |
| | `g_rc_e2_func_id` | 3 | RC function ID |
| **Network** | `maxXAxis` | 4000 m | Simulation area width |
| | `maxYAxis` | 4000 m | Simulation area height |
| | `isd_cell` | 500 m | Inter-site distance for cells |
| | `isd_ue` | 1000 m | Radius for UE distribution |
| **Application** | `PacketSize` | 200 bytes | UDP packet size |
| | `Interval` | 500 μs | Packet interval (2 Mbps) |
| | `simTime` | 1000 s | Simulation time |

---

## 🔍 KPM Metrics Available

### DU Metrics (PHY, MAC, RLC)
- **PHY**: SINR, RSRP, RSRQ, throughput, BLER
- **MAC**: Buffer status, scheduling information, HARQ statistics
- **RLC**: PDU statistics, buffer occupancy, retransmissions

### CU-UP Metrics (PDCP)
- **PDCP**: Throughput, latency, packet loss, PDU statistics

### CU-CP Metrics (RRC)
- **RRC**: Connection status, handover events, cell selection/reselection

---

## ⚙️ Common Modifications

### Change Scenario Type (SA ↔ EN-DC)
```cpp
// SA to EN-DC: Change node counts, device types, E2 mode, add X2 interface
// EN-DC to SA: Change node counts, device types, E2 mode, remove X2 interface
```

### Change Number of Cells and UEs
```cpp
uint8_t nMmWaveEnbNodes = 4;     // Change number of gNBs
uint32_t nUeNodes = 50;          // Change number of UEs
```

### Change Frequency and Bandwidth
```cpp
double centerFrequency = 28e9;   // 28 GHz (mmWave)
double bandwidth = 100e6;        // 100 MHz
```

### Change Mobility Model
```cpp
// Change to constant velocity, random waypoint, etc.
```

### Change Pathloss Model
```cpp
// Change to rural macro, urban macro, etc.
```

### Change E2 Report Periodicity
```cpp
double indicationPeriodicity = 1.0;  // 1 second
```

---

## ✅ Configuration Checklist

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

## 🐛 Troubleshooting

### E2 Interface Not Working
- Check `e2TermIp` is correct
- Check `enableE2FileLogging` setting
- Verify `NodeContainerManager::GetInstance().SetMmWaveEnbNodes()` is called
- Check E2 reports are enabled (`e2du`, `e2cuUp`, `e2cuCp`)

### UEs Not Attaching
- Check device types match scenario (SA vs EN-DC)
- Verify X2 interface is set up for EN-DC
- Check node positions are valid
- Verify EPC is set up correctly

### No Traffic
- Check applications are installed and started
- Verify IP addresses are assigned
- Check routing is configured
- Verify UEs are attached to eNBs

### Handover Not Working
- Check handover mode is set correctly
- Verify SINR difference threshold
- Check X2 interface is set up (for EN-DC)
- Verify handover algorithm is enabled

---

**End of Quick Reference Card**

Use this card as a quick reference when configuring NS-3 scenarios. For detailed explanations, refer to the full NS-3 Component Configuration Guide.


