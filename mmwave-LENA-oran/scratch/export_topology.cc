/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Topology Export Script for Differing_Power_Scenario
 * This script exports node positions to a text file for visualization
 */

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[])
{
    // Default parameters (matching Differing_Power_Scenario.cc)
    double maxXAxis = 2000.0;
    double maxYAxis = 2000.0;
    double isd_cell = 500.0;  // Inter-Site Distance
    uint32_t nUes = 20;
    bool useHybrid = false;
    uint32_t rngRun = 1;
    
    CommandLine cmd;
    cmd.AddValue("N_Ues", "Number of UEs", nUes);
    cmd.AddValue("IntersideDistanceCells", "Inter-Site Distance", isd_cell);
    cmd.AddValue("useHybrid", "Use Hybrid mode (affects BS height)", useHybrid);
    cmd.AddValue("RngRun", "RNG Run ID", rngRun);
    cmd.Parse(argc, argv);
    
    // Set RNG seed (matching the scenario)
    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(rngRun);
    
    // Calculate positions (matching Differing_Power_Scenario.cc)
    Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);
    double enbHeight = useHybrid ? 15.0 : 3.0;
    
    // Base Station positions
    Vector enb1Pos = Vector(centerPosition.x - isd_cell/2, centerPosition.y, enbHeight);
    Vector enb2Pos = Vector(centerPosition.x + isd_cell/2, centerPosition.y, enbHeight);
    
    // Generate UE positions (matching the scenario's random distribution)
    Ptr<UniformRandomVariable> xPos = CreateObject<UniformRandomVariable>();
    xPos->SetAttribute("Min", DoubleValue(750.0));
    xPos->SetAttribute("Max", DoubleValue(1250.0));
    Ptr<UniformRandomVariable> yPos = CreateObject<UniformRandomVariable>();
    yPos->SetAttribute("Min", DoubleValue(980.0));
    yPos->SetAttribute("Max", DoubleValue(1020.0));
    
    // Export to file
    std::ofstream outFile("topology_export.txt");
    if (!outFile.is_open())
    {
        std::cerr << "Error: Cannot open topology_export.txt for writing!" << std::endl;
        return 1;
    }
    
    // Write header
    outFile << "# Topology Export for Differing_Power_Scenario\n";
    outFile << "# Format: Type, NodeID, X(m), Y(m), Z(m), AdditionalInfo\n";
    outFile << "# Generated with: N_Ues=" << nUes << ", ISD=" << isd_cell 
            << "m, BS_Height=" << enbHeight << "m, RngRun=" << rngRun << "\n\n";
    
    // Write Base Stations
    outFile << "# Base Stations (mmWave eNBs)\n";
    outFile << "BS,1," << std::fixed << std::setprecision(2) 
            << enb1Pos.x << "," << enb1Pos.y << "," << enb1Pos.z 
            << ",Cell1\n";
    outFile << "BS,2," << enb2Pos.x << "," << enb2Pos.y << "," << enb2Pos.z 
            << ",Cell2\n";
    outFile << "\n";
    
    // Write UEs
    outFile << "# User Equipment (UEs)\n";
    for (uint32_t i = 0; i < nUes; i++)
    {
        double ueX = xPos->GetValue();
        double ueY = yPos->GetValue();
        double ueZ = 1.5;
        
        // Calculate distance to each BS for attachment info
        double dist1 = std::sqrt(std::pow(ueX - enb1Pos.x, 2) + 
                                std::pow(ueY - enb1Pos.y, 2) + 
                                std::pow(ueZ - enb1Pos.z, 2));
        double dist2 = std::sqrt(std::pow(ueX - enb2Pos.x, 2) + 
                                std::pow(ueY - enb2Pos.y, 2) + 
                                std::pow(ueZ - enb2Pos.z, 2));
        
        std::string attachedCell = (dist1 <= dist2) ? "Cell1" : "Cell2";
        double minDist = (dist1 <= dist2) ? dist1 : dist2;
        
        outFile << "UE," << (i+1) << "," << std::fixed << std::setprecision(2)
                << ueX << "," << ueY << "," << ueZ 
                << "," << attachedCell << "_dist=" << std::setprecision(1) << minDist << "m\n";
    }
    
    outFile.close();
    
    // Print summary to console
    std::cout << "\n=== Topology Summary ===\n";
    std::cout << "Base Stations:\n";
    std::cout << "  Cell 1: (" << enb1Pos.x << ", " << enb1Pos.y << ", " << enb1Pos.z << ") m\n";
    std::cout << "  Cell 2: (" << enb2Pos.x << ", " << enb2Pos.y << ", " << enb2Pos.z << ") m\n";
    std::cout << "  Inter-Site Distance: " << isd_cell << " m\n";
    std::cout << "\nUEs: " << nUes << " devices\n";
    std::cout << "  X range: 750.0 - 1250.0 m\n";
    std::cout << "  Y range: 980.0 - 1020.0 m\n";
    std::cout << "  Z (height): 1.5 m\n";
    std::cout << "\nTopology exported to: topology_export.txt\n";
    std::cout << "\n=== Visualization Commands ===\n";
    std::cout << "Python visualization script: python3 visualize_topology.py\n";
    std::cout << "Or use the generated topology_export.txt file\n";
    
    return 0;
}

