#!/bin/bash
# Energy Monitoring Disable Patch
# Apply this to Differing_Power_Scenerio_HO.cc

# Step 1: Add GlobalValue parameter after line 737
# Insert these 5 lines after "...MakeBooleanChecker());" on line 737:
#
# static ns3::GlobalValue g_enableEnergyMonitoring("enableEnergyMonitoring",
#                                                  "If true, enable energy consumption monitoring and CSV output",
#                                                  ns3::BooleanValue(false),
#                                                  ns3::MakeBooleanChecker());
#

# Step 2: Add parameter reading after line 993 
# Insert these 3 lines after "bool reducedPmValues = booleanValue.Get();":
#
# GlobalValue::GetValueByName("enableEnergyMonitoring", booleanValue);
# bool enableEnergyMonitoring = booleanValue.Get();
#

# Step 3: Wrap energy code (lines 1533-1555) in conditional
# BEFORE (line 1533):
#    BasicEnergySourceHelper basicEnergySourceHelper;
#
# AFTER:
#    // Energy monitoring: only enable if explicitly requested (saves ~4.8 GB per 600s scenario)
#    if (enableEnergyMonitoring)
#      {
#        NS_LOG_UNCOND("Energy monitoring ENABLED - will generate energyfilecell*.csv files");
#        
#        BasicEnergySourceHelper basicEnergySourceHelper;
#
# And at the end (after line 1555, after the closing brace of the for loop):
#      }
#    else
#      {
#        NS_LOG_UNCOND("Energy monitoring DISABLED - skipping energyfilecell*.csv generation (saves ~4.8 GB per 600s scenario)");
#      }

echo "Apply these 3 changes manually to the file"
echo "Then rebuild with: ./build.py"
echo "Usage: ./scratch/ns3... --enableEnergyMonitoring=false  (default)"
echo "       ./scratch/ns3... --enableEnergyMonitoring=true   (if needed)"
