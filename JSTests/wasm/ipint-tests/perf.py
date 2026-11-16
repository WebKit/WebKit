#!/usr/local/bin/python3

import math
import re
import subprocess
import sys
import threading
import time

TIMEENV = {
    'DYLD_FRAMEWORK_PATH': '/Volumes/WebKit/DebugVersion/OpenSource/WebKitBuild/Release',
}

COMMONENV = {
    'DYLD_FRAMEWORK_PATH': '/Volumes/WebKit/DebugVersion/OpenSource/WebKitBuild/Release',
    'JS_SHELL_WAIT_FOR_SIGUSR2_TO_EXIT': '1'
}

JSCRELEASE = ['/Volumes/WebKit/DebugVersion/OpenSource/WebKitBuild/Release/jsc', '--validateOptions=1', '--useWasmSIMD=0']
IPINT_RUNS = ['--useWasmIPInt=1', '--useBBQJIT=0', '--useOMGJIT=0']
LLINT_RUNS = ['--useBBQJIT=0', '--useOMGJIT=0']
BBQJIT_RUNS = ['--useWasmIPInt=0', '--useBBQJIT=1', '--useOMGJIT=0']
FILE = ['-m', sys.argv[1]]

MEMORY_SLEEP = 0


def setup_memory_sleep_time():
    start = time.time()
    p = subprocess.Popen(JSCRELEASE + IPINT_RUNS + FILE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=TIMEENV)
    p.communicate()
    runtime = (time.time() - start)
    MEMORY_SLEEP = runtime * 1.5
    print('Timing run: took', runtime, 'seconds to run, setting MEMORY_SLEEP to', MEMORY_SLEEP)


setup_memory_sleep_time()


def measure_memory(pid, result):
    time.sleep(1)
    fp = subprocess.Popen(['footprint', str(pid)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = fp.communicate()
    result.append([
        re.search(r'phys_footprint: ([0-9]+) ([KMG]B)', stdout.decode()),
        re.search(r'phys_footprint_peak: ([0-9]+) ([KMG]B)', stdout.decode())
    ])
    subprocess.Popen(['kill', '-USR2', str(pid)])


def run_ipint():
    p = subprocess.Popen(JSCRELEASE + IPINT_RUNS + FILE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=COMMONENV)
    t = threading.Thread(target=measure_memory, args=[p.pid, ipint_mem])
    t.start()
    stdout, stderr = p.communicate()
    t.join()
    return int(re.search(r'Total execution: ([0-9]+)', stdout.decode())[1]), \
        int(re.search(r'Instantiation time: ([0-9]+)', stdout.decode())[1])


def run_llint():
    p = subprocess.Popen(JSCRELEASE + LLINT_RUNS + FILE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=COMMONENV)
    t = threading.Thread(target=measure_memory, args=[p.pid, llint_mem])
    t.start()
    stdout, stderr = p.communicate()
    t.join()
    stdout, stderr = p.communicate()
    return int(re.search(r'Total execution: ([0-9]+)', stdout.decode())[1]), \
        int(re.search(r'Instantiation time: ([0-9]+)', stdout.decode())[1])


def run_bbq():
    p = subprocess.Popen(JSCRELEASE + BBQJIT_RUNS + FILE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=COMMONENV)
    t = threading.Thread(target=measure_memory, args=[p.pid, bbq_mem])
    t.start()
    stdout, stderr = p.communicate()
    t.join()
    stdout, stderr = p.communicate()
    return int(re.search(r'Total execution: ([0-9]+)', stdout.decode())[1]), \
        int(re.search(r'Instantiation time: ([0-9]+)', stdout.decode())[1])


def run_jsc():
    p = subprocess.Popen(JSCRELEASE + FILE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=COMMONENV)
    t = threading.Thread(target=measure_memory, args=[p.pid, jsc_mem])
    t.start()
    stdout, stderr = p.communicate()
    t.join()
    stdout, stderr = p.communicate()
    return int(re.search(r'Total execution: ([0-9]+)', stdout.decode())[1]), \
        int(re.search(r'Instantiation time: ([0-9]+)', stdout.decode())[1])


ipint_totals = []
llint_totals = []
bbq_totals = []
jsc_totals = []

ipint_mem = []
llint_mem = []
bbq_mem = []
jsc_mem = []

runs = int(sys.argv[2])

RUNS_DONE = 0
TOTAL_RUNS = runs * 4
BAR_WIDTH = 20

START_TIME = None


def progressBar():
    pct = RUNS_DONE / TOTAL_RUNS
    bars = math.floor(pct * BAR_WIDTH)
    empty = BAR_WIDTH - bars
    estimate = MEMORY_SLEEP * TOTAL_RUNS
    if RUNS_DONE != 0:
        estimate = (TOTAL_RUNS - RUNS_DONE) / RUNS_DONE * (time.time() - START_TIME)
    print(f'[{"█" * bars}{" " * empty}] ({RUNS_DONE} / {TOTAL_RUNS}, {pct:.2f}%); {time.time() - START_TIME:.2f} seconds since start (expected finish in {estimate:.2f} s)', end='', flush=True)
    print('\r', end='', flush=True)


START_TIME = time.time()
progressBar()
for _ in range(runs):
    ipint_totals.append(run_ipint())
    RUNS_DONE += 1
    progressBar()
    llint_totals.append(run_llint())
    RUNS_DONE += 1
    progressBar()
    bbq_totals.append(run_bbq())
    RUNS_DONE += 1
    progressBar()
    jsc_totals.append(run_jsc())
    RUNS_DONE += 1
    progressBar()

def memValue(o):
    return int(o[1]) * (0.001 if o[2] == 'KB' else (1 if o[2] == 'MB' else 1000))


print('IPInt:')
print('- average runtime:', sum(a[0] for a in ipint_totals) / len(ipint_totals), f" ms (N = {runs})")
print('- average startup time:', sum(a[1] for a in ipint_totals) / len(ipint_totals), f" ms (N = {runs})")
print('- average peak memory:', sum(memValue(a[1]) for a in ipint_mem) / len(ipint_mem), "MB", f"(N = {runs})")
print('LLInt:')
print('- average runtime:', sum(a[0] for a in llint_totals) / len(llint_totals), f" ms (N = {runs})")
print('- average startup time:', sum(a[1] for a in llint_totals) / len(llint_totals), f" ms (N = {runs})")
print('- average peak memory:', sum(memValue(a[1]) for a in llint_mem) / len(llint_mem), "MB", f"(N = {runs})")
print('BBQ:')
print('- average runtime:', sum(a[0] for a in bbq_totals) / len(bbq_totals), f" ms (N = {runs})")
print('- average startup time:', sum(a[1] for a in bbq_totals) / len(bbq_totals), f" ms (N = {runs})")
print('- average peak memory:', sum(memValue(a[1]) for a in bbq_mem) / len(bbq_mem), "MB", f"(N = {runs})")
print('JSC:')
print('- average runtime:', sum(a[0] for a in jsc_totals) / len(jsc_totals), f" ms (N = {runs})")
print('- average startup time:', sum(a[1] for a in jsc_totals) / len(jsc_totals), f" ms (N = {runs})")
print('- average peak memory:', sum(memValue(a[1]) for a in jsc_mem) / len(jsc_mem), "MB", f"(N = {runs})")
