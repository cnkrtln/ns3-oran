/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2016, University of Padova, Dep. of Information Engineering, SIGNET lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jaume Nin <jnin@cttc.cat>
 *         Nicola Baldo <nbaldo@cttc.cat>
 *
 * Modified by Michele Polese <michele.polese@gmail.com>
 *     (support for RRC_CONNECTED->RRC_IDLE state transition + support for real S1AP link)
 */

#include "epc-enb-application.h"

#include "epc-gtpu-header.h"
#include "eps-bearer-tag.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EpcEnbApplication");

EpcEnbApplication::EpsFlowId_t::EpsFlowId_t()
{
}

EpcEnbApplication::EpsFlowId_t::EpsFlowId_t(const uint16_t a, const uint8_t b)
    : m_rnti(a),
      m_bid(b)
{
}

bool
operator==(const EpcEnbApplication::EpsFlowId_t& a, const EpcEnbApplication::EpsFlowId_t& b)
{
    return ((a.m_rnti == b.m_rnti) && (a.m_bid == b.m_bid));
}

bool
operator<(const EpcEnbApplication::EpsFlowId_t& a, const EpcEnbApplication::EpsFlowId_t& b)
{
    return ((a.m_rnti < b.m_rnti) || ((a.m_rnti == b.m_rnti) && (a.m_bid < b.m_bid)));
}

TypeId
EpcEnbApplication::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::EpcEnbApplication")
            .SetParent<Object>()
            .SetGroupName("Lte")
            .AddTraceSource("RxFromEnb",
                            "Receive data packets from LTE Enb Net Device",
                            MakeTraceSourceAccessor(&EpcEnbApplication::m_rxLteSocketPktTrace),
                            "ns3::EpcEnbApplication::RxTracedCallback")
            .AddTraceSource("RxFromS1u",
                            "Receive data packets from S1-U Net Device",
                            MakeTraceSourceAccessor(&EpcEnbApplication::m_rxS1uSocketPktTrace),
                            "ns3::EpcEnbApplication::RxTracedCallback");
    return tid;
}

void
EpcEnbApplication::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    m_lteSocket = 0;
    m_lteSocket6 = 0;
    m_s1uSocket = 0;
    delete m_s1SapProvider;
    delete m_s1apSapEnb;
}

EpcEnbApplication::EpcEnbApplication(Ptr<Socket> lteSocket,
                                     Ptr<Socket> lteSocket6,
                                     Ptr<Socket> s1uSocket,
                                     Ipv4Address enbS1uAddress,
                                     Ipv4Address sgwS1uAddress,
                                     uint16_t cellId)
    : m_lteSocket(lteSocket),
      m_lteSocket6(lteSocket6),
      m_s1uSocket(s1uSocket),
      m_enbS1uAddress(enbS1uAddress),
      m_sgwS1uAddress(sgwS1uAddress),
      m_gtpuUdpPort(2152), // fixed by the standard
      m_s1SapUser(0),
      m_s1apSapEnbProvider(0),
      m_cellId(cellId)
{
    NS_LOG_FUNCTION(this << lteSocket << s1uSocket << sgwS1uAddress);
    m_s1uSocket->SetRecvCallback(MakeCallback(&EpcEnbApplication::RecvFromS1uSocket, this));
    m_lteSocket->SetRecvCallback(MakeCallback(&EpcEnbApplication::RecvFromLteSocket, this));
    m_lteSocket6->SetRecvCallback(MakeCallback(&EpcEnbApplication::RecvFromLteSocket, this));
    m_s1SapProvider = new MemberEpcEnbS1SapProvider<EpcEnbApplication>(this);
    m_s1apSapEnb = new MemberEpcS1apSapEnb<EpcEnbApplication>(this);
}

EpcEnbApplication::~EpcEnbApplication(void)
{
    NS_LOG_FUNCTION(this);
}

void
EpcEnbApplication::SetS1SapUser(EpcEnbS1SapUser* s)
{
    m_s1SapUser = s;
}

EpcEnbS1SapProvider*
EpcEnbApplication::GetS1SapProvider()
{
    return m_s1SapProvider;
}

void
EpcEnbApplication::SetS1apSapMme(EpcS1apSapEnbProvider* s)
{
    m_s1apSapEnbProvider = s;
}

EpcS1apSapEnb*
EpcEnbApplication::GetS1apSapEnb()
{
    return m_s1apSapEnb;
}

void
EpcEnbApplication::DoInitialUeMessage(uint64_t imsi, uint16_t rnti)
{
    NS_LOG_FUNCTION(this);
    // side effect: create entry if not exist
    m_imsiRntiMap[imsi] = rnti;
    m_s1apSapEnbProvider->SendInitialUeMessage(
        imsi,
        rnti,
        imsi,
        m_cellId); // TODO if more than one MME is used, extend this call
}

void
EpcEnbApplication::DoPathSwitchRequest(EpcEnbS1SapProvider::PathSwitchRequestParameters params)
{
    NS_LOG_FUNCTION(this);
    uint16_t enbUeS1Id = params.rnti;
    uint64_t mmeUeS1Id = params.mmeUeS1Id;
    uint64_t imsi = mmeUeS1Id;
    // side effect: create entry if not exist
    m_imsiRntiMap[imsi] = params.rnti;

    uint16_t gci = params.cellId;
    std::list<EpcS1apSapMme::ErabSwitchedInDownlinkItem> erabToBeSwitchedInDownlinkList;
    for (std::list<EpcEnbS1SapProvider::BearerToBeSwitched>::iterator bit =
             params.bearersToBeSwitched.begin();
         bit != params.bearersToBeSwitched.end();
         ++bit)
    {
        uint32_t teid = bit->teid;
        
        // CRITICAL FIX: Check if mapping already exists (from UpdateRntiForTeid in manual handover)
        // If it exists, use the BID from the existing mapping (which is the correct m_epsBearerIdentity)
        // Otherwise, use the epsBearerId from PathSwitchRequest
        uint8_t bid = bit->epsBearerId; // Default to epsBearerId
        std::map<uint32_t, EpsFlowId_t>::iterator teidIt = m_teidRbidMap.find(teid);
        if (teidIt != m_teidRbidMap.end())
        {
            // Mapping already exists (from UpdateRntiForTeid) - use its BID (which is m_epsBearerIdentity)
            uint8_t oldBid = teidIt->second.m_bid;
            bid = teidIt->second.m_bid;
            // Update RNTI if it changed
            teidIt->second.m_rnti = params.rnti;
            
            // DEBUG: Log BID preservation
            double timestamp = Simulator::Now().GetSeconds();
            std::string handoverDebugFileName = "/dev/null";
            std::ofstream handoverDebugLog(handoverDebugFileName.c_str(), std::ios::app);
            if (handoverDebugLog.is_open())
            {
                handoverDebugLog << timestamp << ",PATH_SWITCH_PRESERVED_BID,Cell" << m_cellId
                                 << ",IMSI" << imsi << ",TEID=" << teid << ",RNTI=" << params.rnti
                                 << ",OldBID=" << (uint32_t)oldBid << ",PreservedBID=" << (uint32_t)bid
                                 << ",EpsBearerId=" << (uint32_t)bit->epsBearerId << std::endl;
                handoverDebugLog.close();
            }
        }
        else
        {
            // DEBUG: Log BID from PathSwitchRequest (mapping didn't exist)
            double timestamp = Simulator::Now().GetSeconds();
            std::string handoverDebugFileName = "/dev/null";
            std::ofstream handoverDebugLog(handoverDebugFileName.c_str(), std::ios::app);
            if (handoverDebugLog.is_open())
            {
                handoverDebugLog << timestamp << ",PATH_SWITCH_NEW_MAPPING,Cell" << m_cellId
                                 << ",IMSI" << imsi << ",TEID=" << teid << ",RNTI=" << params.rnti
                                 << ",BID=" << (uint32_t)bid << ",EpsBearerId=" << (uint32_t)bit->epsBearerId << std::endl;
                handoverDebugLog.close();
            }
        }
        
        EpsFlowId_t flowId;
        flowId.m_rnti = params.rnti;
        flowId.m_bid = bid;
        
        EpsFlowId_t rbid(params.rnti, bid);
        // side effect: create entries if not exist
        m_rbidTeidMap[params.rnti][bid] = teid;
        m_teidRbidMap[teid] = rbid;

        EpcS1apSapMme::ErabSwitchedInDownlinkItem erab;
        erab.erabId = bit->epsBearerId;
        erab.enbTransportLayerAddress = m_enbS1uAddress;
        erab.enbTeid = bit->teid;

        erabToBeSwitchedInDownlinkList.push_back(erab);
    }
    m_s1apSapEnbProvider->SendPathSwitchRequest(enbUeS1Id,
                                                mmeUeS1Id,
                                                gci,
                                                erabToBeSwitchedInDownlinkList);
}

void
EpcEnbApplication::DoUeContextRelease(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    std::map<uint16_t, std::map<uint8_t, uint32_t>>::iterator rntiIt = m_rbidTeidMap.find(rnti);
    if (rntiIt != m_rbidTeidMap.end())
    {
        for (std::map<uint8_t, uint32_t>::iterator bidIt = rntiIt->second.begin();
             bidIt != rntiIt->second.end();
             ++bidIt)
        {
            uint32_t teid = bidIt->second;
            m_teidRbidMap.erase(teid);
        }
        m_rbidTeidMap.erase(rntiIt);
    }
}

void
EpcEnbApplication::DoInitialContextSetupRequest(
    uint64_t mmeUeS1Id,
    uint16_t enbUeS1Id,
    std::list<EpcS1apSapEnb::ErabToBeSetupItem> erabToBeSetupList)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO(
        "In EnpEnbApplication DoInitialContextSetupRequest size of the erabToBeSetupList is "
        << erabToBeSetupList.size());

    for (std::list<EpcS1apSapEnb::ErabToBeSetupItem>::iterator erabIt = erabToBeSetupList.begin();
         erabIt != erabToBeSetupList.end();
         ++erabIt)
    {
        // request the RRC to setup a radio bearer
        uint64_t imsi = mmeUeS1Id;
        std::map<uint64_t, uint16_t>::iterator imsiIt = m_imsiRntiMap.find(imsi);
        NS_ASSERT_MSG(imsiIt != m_imsiRntiMap.end(), "unknown IMSI");
        uint16_t rnti = imsiIt->second;

        struct EpcEnbS1SapUser::DataRadioBearerSetupRequestParameters params;
        params.rnti = rnti;
        params.bearer = erabIt->erabLevelQosParameters;
        params.bearerId = erabIt->erabId;
        params.gtpTeid = erabIt->sgwTeid;
        m_s1SapUser->DataRadioBearerSetupRequest(params);

        EpsFlowId_t rbid(rnti, erabIt->erabId);
        // side effect: create entries if not exist
        m_rbidTeidMap[rnti][erabIt->erabId] = params.gtpTeid;
        m_teidRbidMap[params.gtpTeid] = rbid;
    }
}

void
EpcEnbApplication::DoPathSwitchRequestAcknowledge(
    uint64_t enbUeS1Id,
    uint64_t mmeUeS1Id,
    uint16_t gci,
    std::list<EpcS1apSapEnb::ErabSwitchedInUplinkItem> erabToBeSwitchedInUplinkList)
{
    NS_LOG_FUNCTION(this);

    uint64_t imsi = mmeUeS1Id;
    
    // DEBUG: Log PathSwitchRequestAcknowledge received
    double timestamp = Simulator::Now().GetSeconds();
    std::string handoverDebugFileName = "/dev/null";
    std::ofstream handoverDebugLog(handoverDebugFileName.c_str(), std::ios::app);
    if (handoverDebugLog.is_open())
      {
        handoverDebugLog << timestamp << ",ENB_PATH_SWITCH_ACK_RECEIVED,Cell" << m_cellId 
                         << ",IMSI" << imsi << ",EnbUeS1Id=" << enbUeS1Id 
                         << ",CellId=" << gci << std::endl;
        handoverDebugLog.close();
      }
    
    std::map<uint64_t, uint16_t>::iterator imsiIt = m_imsiRntiMap.find(imsi);
    NS_ASSERT_MSG(imsiIt != m_imsiRntiMap.end(), "unknown IMSI");
    uint16_t rnti = imsiIt->second;
    EpcEnbS1SapUser::PathSwitchRequestAcknowledgeParameters params;
    params.rnti = rnti;
    
    // DEBUG: Log forwarding to RRC
    timestamp = Simulator::Now().GetSeconds();
    handoverDebugLog.open(handoverDebugFileName.c_str(), std::ios::app);
    if (handoverDebugLog.is_open())
      {
        handoverDebugLog << timestamp << ",ENB_PATH_SWITCH_ACK_FORWARDING,Cell" << m_cellId 
                         << ",IMSI" << imsi << ",RNTI" << rnti << std::endl;
        handoverDebugLog.close();
      }
    
    m_s1SapUser->PathSwitchRequestAcknowledge(params);
}

void
EpcEnbApplication::RecvFromLteSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this);
    if (m_lteSocket6)
    {
        NS_ASSERT(socket == m_lteSocket || socket == m_lteSocket6);
    }
    else
    {
        NS_ASSERT(socket == m_lteSocket);
    }
    Ptr<Packet> packet = socket->Recv();

    /// \internal
    /// Workaround for \bugid{231}
    // SocketAddressTag satag;
    // packet->RemovePacketTag (satag);

    EpsBearerTag tag;
    bool found = packet->RemovePacketTag(tag);
    NS_ASSERT(found);
    uint16_t rnti = tag.GetRnti();
    uint8_t bid = tag.GetBid();
    NS_LOG_LOGIC("received packet with RNTI=" << (uint32_t)rnti << ", BID=" << (uint32_t)bid);
    std::map<uint16_t, std::map<uint8_t, uint32_t>>::iterator rntiIt = m_rbidTeidMap.find(rnti);
    if (rntiIt == m_rbidTeidMap.end())
    {
        NS_LOG_WARN("UE context not found, discarding packet when receiving from lteSocket");
    }
    else
    {
        std::map<uint8_t, uint32_t>::iterator bidIt = rntiIt->second.find(bid);
        NS_ASSERT(bidIt != rntiIt->second.end());
        uint32_t teid = bidIt->second;
        m_rxLteSocketPktTrace(packet->Copy());
        SendToS1uSocket(packet, teid);
    }
}

void
EpcEnbApplication::RecvFromS1uSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_ASSERT(socket == m_s1uSocket);
    Ptr<Packet> packet = socket->Recv();
    GtpuHeader gtpu;
    packet->RemoveHeader(gtpu);
    uint32_t teid = gtpu.GetTeid();

    /// \internal
    /// Workaround for \bugid{231}
    // SocketAddressTag tag;
    // packet->RemovePacketTag (tag);

    std::map<uint32_t, EpsFlowId_t>::iterator it = m_teidRbidMap.find(teid);
    if (it != m_teidRbidMap.end())
    {
        // DEBUG: Log successful packet processing
        double timestamp = Simulator::Now().GetSeconds();
        std::string handoverDebugFileName = "/dev/null";
        std::ofstream handoverDebugLog(handoverDebugFileName.c_str(), std::ios::app);
        if (handoverDebugLog.is_open())
          {
            handoverDebugLog << timestamp << ",S1U_PACKET_PROCESSED,Cell" << m_cellId 
                             << ",TEID=" << teid << ",RNTI=" << it->second.m_rnti 
                             << ",BID=" << (uint32_t)it->second.m_bid << ",Size=" << packet->GetSize() << std::endl;
            handoverDebugLog.close();
          }
        
        m_rxS1uSocketPktTrace(packet->Copy());
        SendToLteSocket(packet, it->second.m_rnti, it->second.m_bid);
    }
    else
    {
        // DEBUG: Log discarded packets (TEID mapping not found)
        double timestamp = Simulator::Now().GetSeconds();
        std::string handoverDebugFileName = "/dev/null";
        std::ofstream handoverDebugLog(handoverDebugFileName.c_str(), std::ios::app);
        if (handoverDebugLog.is_open())
          {
            handoverDebugLog << timestamp << ",S1U_PACKET_DISCARDED,Cell" << m_cellId 
                             << ",TEID=" << teid << ",Reason=TEID_MAPPING_NOT_FOUND" << std::endl;
            handoverDebugLog.close();
          }
        packet = 0;
        NS_LOG_DEBUG("UE context not found, discarding packet when receiving from s1uSocket");
    }
}

void
EpcEnbApplication::SendToLteSocket(Ptr<Packet> packet, uint16_t rnti, uint8_t bid)
{
    NS_LOG_FUNCTION(this << packet << rnti << (uint16_t)bid << packet->GetSize());
    EpsBearerTag tag(rnti, bid);
    packet->AddPacketTag(tag);
    uint8_t ipType;

    packet->CopyData(&ipType, 1);
    ipType = (ipType >> 4) & 0x0f;

    int sentBytes;
    if (ipType == 0x04)
    {
        sentBytes = m_lteSocket->Send(packet);
    }
    else if (ipType == 0x06)
    {
        sentBytes = m_lteSocket6->Send(packet);
    }
    else
    {
        NS_ABORT_MSG("EpcEnbApplication::SendToLteSocket - Unknown IP type...");
    }

    NS_ASSERT(sentBytes > 0);
}

void
EpcEnbApplication::SendToS1uSocket(Ptr<Packet> packet, uint32_t teid)
{
    NS_LOG_FUNCTION(this << packet << teid << packet->GetSize());
    GtpuHeader gtpu;
    gtpu.SetTeid(teid);
    // From 3GPP TS 29.281 v10.0.0 Section 5.1
    // Length of the payload + the non obligatory GTP-U header
    gtpu.SetLength(packet->GetSize() + gtpu.GetSerializedSize() - 8);
    packet->AddHeader(gtpu);
    uint32_t flags = 0;
    m_s1uSocket->SendTo(packet, flags, InetSocketAddress(m_sgwS1uAddress, m_gtpuUdpPort));
}

void
EpcEnbApplication::DoReleaseIndication(uint64_t imsi, uint16_t rnti, uint8_t bearerId)
{
    NS_LOG_FUNCTION(this << bearerId);
    std::list<EpcS1apSapMme::ErabToBeReleasedIndication> erabToBeReleaseIndication;
    EpcS1apSapMme::ErabToBeReleasedIndication erab;
    erab.erabId = bearerId;
    erabToBeReleaseIndication.push_back(erab);
    // From 3GPP TS 23401-950 Section 5.4.4.2, enB sends EPS bearer Identity in Bearer Release
    // Indication message to MME
    m_s1apSapEnbProvider->SendErabReleaseIndication(imsi, rnti, erabToBeReleaseIndication);
}

void
EpcEnbApplication::UpdateRntiForTeid(uint32_t teid, uint16_t oldRnti, uint16_t newRnti, uint8_t bid)
{
    NS_LOG_FUNCTION(this << teid << oldRnti << newRnti << (uint16_t)bid);
    
    // DEBUG: Log to file
    double timestamp = Simulator::Now().GetSeconds();
    std::string handoverDebugFileName = "/dev/null";
    std::ofstream handoverDebugLog(handoverDebugFileName.c_str(), std::ios::app);
    
    // Update m_teidRbidMap: Change RNTI AND BID for this TEID
    // If mapping doesn't exist, CREATE it (for manual handover where mapping might not exist in target cell)
    std::map<uint32_t, EpsFlowId_t>::iterator teidIt = m_teidRbidMap.find(teid);
    if (teidIt != m_teidRbidMap.end())
    {
        // CRITICAL FIX: Update BOTH RNTI AND BID in the flow ID
        // The BID must match m_epsBearerIdentity from the target DRB, otherwise packets
        // will arrive with the wrong BID and won't be routed correctly
        uint8_t oldBid = teidIt->second.m_bid;
        teidIt->second.m_rnti = newRnti;
        teidIt->second.m_bid = bid;  // Update BID to match m_epsBearerIdentity
        NS_LOG_INFO("EpcEnbApplication: Updated m_teidRbidMap[TEID=" << teid 
                    << "] from RNTI " << oldRnti << " to RNTI " << newRnti 
                    << ", BID from " << (uint16_t)oldBid << " to " << (uint16_t)bid);
        
        if (handoverDebugLog.is_open())
          {
            handoverDebugLog << timestamp << ",EPC_UPDATE_TEID_FOUND,Cell" << m_cellId 
                             << ",TEID=" << teid << ",OldRNTI=" << oldRnti 
                             << ",NewRNTI=" << newRnti << ",OldBID=" << (uint32_t)oldBid
                             << ",NewBID=" << (uint32_t)bid << ",Action=UPDATED" << std::endl;
            handoverDebugLog.close();
          }
    }
    else
    {
        // CREATE the mapping if it doesn't exist (for manual handover)
        // This is similar to DoPathSwitchRequest and DoInitialContextSetupRequest
        EpsFlowId_t rbid(newRnti, bid);
        m_teidRbidMap[teid] = rbid;
        NS_LOG_INFO("EpcEnbApplication: Created m_teidRbidMap[TEID=" << teid 
                    << "] with RNTI " << newRnti << " (mapping did not exist)");
        
        if (handoverDebugLog.is_open())
          {
            handoverDebugLog << timestamp << ",EPC_UPDATE_TEID_NOT_FOUND,Cell" << m_cellId 
                             << ",TEID=" << teid << ",OldRNTI=" << oldRnti 
                             << ",NewRNTI=" << newRnti << ",Action=CREATED" << std::endl;
            handoverDebugLog.close();
          }
    }
    
    // Update m_rbidTeidMap: Remove old entry, add new entry
    // Remove old RNTI->BID->TEID mapping (only if oldRnti != newRnti to avoid removing what we just added)
    if (oldRnti != newRnti)
      {
        std::map<uint16_t, std::map<uint8_t, uint32_t>>::iterator rntiIt = m_rbidTeidMap.find(oldRnti);
        if (rntiIt != m_rbidTeidMap.end())
          {
            std::map<uint8_t, uint32_t>::iterator bidIt = rntiIt->second.find(bid);
            if (bidIt != rntiIt->second.end() && bidIt->second == teid)
              {
                rntiIt->second.erase(bidIt);
                NS_LOG_INFO("EpcEnbApplication: Removed m_rbidTeidMap[RNTI=" << oldRnti 
                            << "][BID=" << (uint16_t)bid << "] = TEID " << teid);
                
                // If this was the last BID for this RNTI, remove the RNTI entry
                if (rntiIt->second.empty())
                  {
                    m_rbidTeidMap.erase(rntiIt);
                  }
              }
          }
      }
    
    // Add new RNTI->BID->TEID mapping
    m_rbidTeidMap[newRnti][bid] = teid;
    NS_LOG_INFO("EpcEnbApplication: Added m_rbidTeidMap[RNTI=" << newRnti 
                << "][BID=" << (uint16_t)bid << "] = TEID " << teid);
}

void
EpcEnbApplication::RemoveTeidMapping(uint32_t teid, uint16_t rnti, uint8_t bid)
{
    NS_LOG_FUNCTION(this << teid << rnti << (uint16_t)bid);
    
    // Remove from m_teidRbidMap
    std::map<uint32_t, EpsFlowId_t>::iterator teidIt = m_teidRbidMap.find(teid);
    if (teidIt != m_teidRbidMap.end())
    {
        NS_LOG_INFO("EpcEnbApplication: Removing m_teidRbidMap[TEID=" << teid 
                    << "] (RNTI " << teidIt->second.m_rnti << ", BID " << (uint16_t)teidIt->second.m_bid << ")");
        m_teidRbidMap.erase(teidIt);
    }
    else
    {
        NS_LOG_WARN("EpcEnbApplication: RemoveTeidMapping - TEID " << teid << " not found in m_teidRbidMap");
    }
    
    // Remove from m_rbidTeidMap
    std::map<uint16_t, std::map<uint8_t, uint32_t>>::iterator rntiIt = m_rbidTeidMap.find(rnti);
    if (rntiIt != m_rbidTeidMap.end())
    {
        std::map<uint8_t, uint32_t>::iterator bidIt = rntiIt->second.find(bid);
        if (bidIt != rntiIt->second.end() && bidIt->second == teid)
        {
            NS_LOG_INFO("EpcEnbApplication: Removing m_rbidTeidMap[RNTI=" << rnti 
                        << "][BID=" << (uint16_t)bid << "] = TEID " << teid);
            rntiIt->second.erase(bidIt);
            
            // If this was the last BID for this RNTI, remove the RNTI entry
            if (rntiIt->second.empty())
            {
                m_rbidTeidMap.erase(rntiIt);
            }
        }
    }
}

} // namespace ns3
