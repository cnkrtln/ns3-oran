#!/usr/bin/env python3
import os
import time
import random
import subprocess
import threading

# Directory where we want all NS-3 outputs to be generated
SIM_DIR = "ns3_sim_outputs"

# --- Scenario Configuration ---
ACTION_INTERVAL_SIM_SECS = 1.0  # Apply action every 1.0 simulation seconds

# Constraints / Steps from run_parallel_scenarios
TX_MIN, TX_MAX = 30.0, 46.0
TX_STEP = 4.0

TILT_MIN, TILT_MAX = 5.0, 15.0
TILT_STEP = 2.5

A3_VALUES = [3.0, 0.0, -3.0]

# Ensure the output directory exists
os.makedirs(SIM_DIR, exist_ok=True)

# NS-3 command
NS3_CMD = [
    "../build/scratch/ns3.42-Differing_Power_Scenerio_HO-optimized",
    "--simTime=10.0",
    "--N_Ues=20",
    "--useHybrid=true",
    "--enableTiltTwoRay=true",
    "--reducedPmValues=true",
    "--enableE2FileLogging=true",
    "--enableTraces=false",
    "--Mobility=true",
    "--exportUEPositions=true",
    "--enableRuntimeControl=true",
    "--ControlPollInterval=10",
    "--RngRun=1444",
    "--TxPower1=38.0",
    "--TxPower2=38.0",
    "--Tilt=10.0"
]

def run_ns3():
    """Runs the NS-3 simulation in the background inside SIM_DIR"""
    print(f"[NS-3] Starting simulation inside ./{SIM_DIR}/ ...")
    
    log_file_path = os.path.join(SIM_DIR, "ns3_stdout.log")
    with open(log_file_path, "w") as out:
        process = subprocess.Popen(NS3_CMD, cwd=SIM_DIR, stdout=out, stderr=subprocess.STDOUT)
        process.wait()
        print(f"\n[NS-3] Simulation finished! Outputs saved in ./{SIM_DIR}/")

def write_commands(commands):
    """Write commands atomically to runtime_control.txt in SIM_DIR"""
    if not commands:
        return
        
    control_file = os.path.join(SIM_DIR, "runtime_control.txt")
    temp_file = f"{control_file}.tmp"
    
    try:
        with open(temp_file, 'w') as f:
            for cmd in commands:
                f.write(f"{cmd['type']} {cmd['cell']} {cmd['value']:.2f}\n")
        
        os.rename(temp_file, control_file)
        
        for cmd in commands:
            print(f"[xApp] Changed {cmd['type']} on Cell {cmd['cell']} to {cmd['value']:.2f}")
            
    except Exception as e:
        print(f"[xApp] Error writing commands: {e}")

def get_current_sim_time(position_file):
    """Reads the last line of UEPosition.txt to find current simulation time."""
    if not os.path.exists(position_file):
        return -1.0
    try:
        with open(position_file, 'rb') as f:
            try:
                f.seek(-1024, os.SEEK_END)
            except OSError:
                f.seek(0)
            
            last_lines = f.readlines()
            if not last_lines:
                return -1.0
                
            for line in reversed(last_lines):
                try:
                    parts = line.decode('utf-8').strip().split(',')
                    if len(parts) > 0:
                        return float(parts[0])
                except (ValueError, IndexError):
                    continue
    except Exception:
        pass
    return -1.0

def xapp_loop():
    """Stateful xApp logic synchronized with sim_time"""
    print("[Dummy-xApp] Starting random stateful parameter generation...")
    time.sleep(2) # Wait for NS-3 to create files
    
    position_file = os.path.join(SIM_DIR, "UEPosition.txt")
    next_action_time = ACTION_INTERVAL_SIM_SECS
    
    # Internal State tracking (like run_parallel_scenarios.py)
    state = {
        1: {'tx': 38.0, 'tilt': 10.0, 'a3': 3.0},
        2: {'tx': 38.0, 'tilt': 10.0, 'a3': 3.0}
    }
    
    while True:
        sim_time = get_current_sim_time(position_file)
        
        # Stop loop if simulation seems finished or we hit 10s
        if sim_time >= 9.9:
            break
            
        if sim_time >= next_action_time:
            print(f"\n[xApp] Action Triggered at Sim Time: {sim_time:.2f}s")
            
            cell_id = random.choice([1, 2])
            other_cell_id = 2 if cell_id == 1 else 1
            param = random.choice(['tx', 'tilt', 'a3'])
            commands = []
            
            if param == 'tx':
                current = state[cell_id]['tx']
                direction = random.choice([-1, 1])
                new_val = current + (direction * TX_STEP)
                
                # Clamp and bounce
                if new_val > TX_MAX: new_val = TX_MAX
                if new_val < TX_MIN: new_val = TX_MIN
                if new_val == current:
                    new_val = current - (direction * TX_STEP)
                
                state[cell_id]['tx'] = new_val
                commands.append({'type': 'POWER', 'cell': cell_id, 'value': new_val})
                
            elif param == 'tilt':
                current = state[cell_id]['tilt']
                direction = random.choice([-1, 1])
                new_val = current + (direction * TILT_STEP)
                
                # Clamp and bounce
                if new_val > TILT_MAX: new_val = TILT_MAX
                if new_val < TILT_MIN: new_val = TILT_MIN
                if new_val == current:
                    new_val = current - (direction * TILT_STEP)
                    
                state[cell_id]['tilt'] = new_val
                commands.append({'type': 'TILT', 'cell': cell_id, 'value': new_val})
                
            elif param == 'a3':
                other_a3 = state[other_cell_id]['a3']
                valid_choices = [v for v in A3_VALUES if not (v == -3.0 and other_a3 == -3.0) and v != state[cell_id]['a3']]
                
                if valid_choices:
                    new_val = random.choice(valid_choices)
                    state[cell_id]['a3'] = new_val
                    commands.append({'type': 'A3', 'cell': cell_id, 'value': new_val})
            
            if commands:
                write_commands(commands)
                
            next_action_time += ACTION_INTERVAL_SIM_SECS
            
        time.sleep(0.1) # Poll fast enough to catch the interval

def main():
    ns3_thread = threading.Thread(target=run_ns3)
    ns3_thread.start()
    xapp_loop()
    ns3_thread.join()

if __name__ == "__main__":
    main()
