#!/usr/bin/env python3

import subprocess
import os
import time

# Set up environment
env = os.environ.copy()
env['DYLD_FRAMEWORK_PATH'] = '/Users/yijiahuang/git/WebKit/OpenSource/WebKitBuild/Debug'

# Change to the test directory
os.chdir('/Users/yijiahuang/git/WebKit/OpenSource/JSTests/wasm/debugger/add')

# Start the debugger process
cmd = ['/Users/yijiahuang/git/WebKit/OpenSource/WebKitBuild/Debug/jsc', '--wasm-debug=12346', 'main.js']

print("Starting debugger process...")
print(f"Command: {' '.join(cmd)}")
print(f"Working directory: {os.getcwd()}")

try:
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env
    )
    
    print(f"Process started with PID: {process.pid}")
    
    # Wait a bit for the process to start and potentially output something
    time.sleep(2)
    
    # Check if process is still running
    if process.poll() is None:
        print("Process is still running, waiting for output...")
        
        # Try to read with timeout
        try:
            stdout, stderr = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            print("Process is still running after 5 seconds, terminating...")
            process.terminate()
            stdout, stderr = process.communicate()
    else:
        stdout, stderr = process.communicate()
    
    print(f"\n=== STDOUT (length: {len(stdout)}) ===")
    if stdout:
        print(repr(stdout))
        print("STDOUT content:")
        print(stdout)
    else:
        print("No stdout output")
    
    print(f"\n=== STDERR (length: {len(stderr)}) ===")
    if stderr:
        print(repr(stderr))
        print("STDERR content:")
        print(stderr)
    else:
        print("No stderr output")
        
    print(f"\nProcess exit code: {process.returncode}")

except Exception as e:
    print(f"Error: {e}")