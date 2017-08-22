/*

Copyright (c) 2017, NVIDIA Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef WIN32
#define _WIN32_WINNT 0x0602
#endif

#include <numeric>
#include <string>
#include <chrono>
#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <semaphore>
#include <algorithm>
#include <set>
#include <map>
#include <iomanip>
#include <fstream>
#include <cmath>

#ifndef __has_include
  #define __has_include(x) 0
#endif

#ifndef __test_abi
  #define __test_abi
  #define __managed__
  template<class T>
  using atomic = std::atomic<T>;
  using mutex = std::mutex;
  using thread = std::thread;
  using namespace std::experimental;
  template<class F>
  int start_gpu_threads(uint32_t count, F) { assert(!count); return 0; }
  void stop_gpu_threads(int) { }
  uint32_t max_gpu_threads() { return 0; }
  unsigned int dev = 0;
  unsigned int cap = 0;
#endif

#include "test.hpp"

#if defined(__linux__) || defined(__APPLE__)
  #include <unistd.h>
  #include <cstring>
  #include <sys/times.h>
  #include <sys/time.h>
  #include <sys/resource.h>

  typedef rusage cpu_time;
  cpu_time get_cpu_time() {

    cpu_time c;
    std::memset(&c, 0, sizeof(cpu_time));
    assert(0 == getrusage(RUSAGE_SELF, &c));

    return c;
  }
  double user_time_consumed(cpu_time start, cpu_time end) {

      return (end.ru_utime.tv_sec + 1E-6*end.ru_utime.tv_usec) -
             (start.ru_utime.tv_sec + 1E-6*start.ru_utime.tv_usec);
  }
  double system_time_consumed(cpu_time start, cpu_time end) {

      return (end.ru_stime.tv_sec + 1E-6*end.ru_stime.tv_usec) -
             (start.ru_stime.tv_sec + 1E-6*start.ru_stime.tv_usec);
  }
#endif

#ifdef __linux__
  #include <sched.h>
  void set_affinity(std::uint64_t cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    cpu %= sizeof(int) * 8;
    CPU_SET(cpu, &cpuset);

    sched_setaffinity(0, sizeof(cpuset), &cpuset);
  }
#endif

#ifdef __APPLE__
  #include <mach/thread_policy.h>
  #include <pthread.h>

  extern "C" kern_return_t thread_policy_set(thread_t                thread,
    thread_policy_flavor_t  flavor,
    thread_policy_t         policy_info,
    mach_msg_type_number_t  count);

  void set_affinity(std::uint64_t cpu) {
    cpu %= sizeof(integer_t) * 8;
    integer_t count = (1 << cpu);
    thread_policy_set(pthread_mach_thread_np(pthread_self()), THREAD_AFFINITY_POLICY, (thread_policy_t)&count, 1);
  }

#if __has_include(<os/lock.h>)
  #include <os/lock.h>
  #define HAS_UNFAIR_LOCK 1
  class unfair_lock {
    os_unfair_lock l = OS_UNFAIR_LOCK_INIT;
  public:
    void lock() { os_unfair_lock_lock(&l); }
    void unlock() { os_unfair_lock_unlock(&l); }
  };
  #endif
#endif

#ifdef WIN32
  static HANDLE self = GetCurrentProcess();
  typedef std::pair<FILETIME, FILETIME> cpu_time;
  cpu_time get_cpu_time() {
    cpu_time t;
    FILETIME ftime, fsys, fuser;
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&t.first, &fsys, sizeof(FILETIME));
    memcpy(&t.second, &fuser, sizeof(FILETIME));
    return t;
  }
  std::uint64_t make64(std::uint64_t low, std::uint64_t high) {
    return low | (high << 32);
  }
  std::uint64_t make64(FILETIME ftime) {
    return make64(ftime.dwLowDateTime, ftime.dwHighDateTime);
  }
  double user_time_consumed(cpu_time start, cpu_time end) {

    double nanoseconds_per_clock_tick = 100; //100-nanosecond intervals
    auto clock_ticks_elapsed = make64(end.second) - make64(start.second);
    return clock_ticks_elapsed * nanoseconds_per_clock_tick;
  }
  double system_time_consumed(cpu_time start, cpu_time end) {

    double nanoseconds_per_clock_tick = 100; //100-nanosecond intervals
    auto clock_ticks_elapsed = make64(end.first) - make64(start.first);
    return clock_ticks_elapsed * nanoseconds_per_clock_tick;
  }
  void set_affinity(std::uint64_t cpu) {
    cpu %= sizeof(std::uint64_t) * 8;
    SetThreadAffinityMask(GetCurrentThread(), int(1 << cpu));
  }
#endif

std::ofstream csv("output.csv");

struct time_record {
    std::chrono::microseconds us;
    double user_time;
    double system_time;
};

static constexpr uint32_t MAX_CPU_THREADS = 1024;
static constexpr uint32_t MAX_GPU_THREADS = 1024*160;

struct work_item_struct {

    atomic<int> cpu_keep_going                    = ATOMIC_VAR_INIT(0);
    unsigned char pad3[4096]                      = {0};
    atomic<int> gpu_keep_going                    = ATOMIC_VAR_INIT(0);
    unsigned char pad35[4096]                     = {0};
    atomic<uint64_t> cpu_count[MAX_CPU_THREADS]   = {ATOMIC_VAR_INIT(0)};
    unsigned char pad4[4096]                      = {0};
    atomic<uint64_t> gpu_count[MAX_GPU_THREADS]   = {ATOMIC_VAR_INIT(0)};
    unsigned char pad5[4096]                      = {0};

    __test_abi int do_it(uint32_t index, bool is_cpu) {

        if (is_cpu) {
            cpu_count[index].fetch_add(1, std::memory_order_relaxed);
            return cpu_keep_going.load(std::memory_order_relaxed);
        }
        else {
            gpu_count[index].fetch_add(1, std::memory_order_relaxed);
            return gpu_keep_going.load(std::memory_order_relaxed);
        }
    
    }
};

work_item_struct* allocate_work_item() {
    work_item_struct* h;
#ifdef __NVCC__
    if(cap < 6)
        cudaHostAlloc(&h, sizeof(work_item_struct), 0);
    else {
        cudaMallocManaged(&h, sizeof(work_item_struct));
        cudaMemAdvise(h, sizeof(work_item_struct), cudaMemAdviseSetPreferredLocation, 0);
    }
#else
    h = (work_item_struct*)malloc(sizeof(work_item_struct));
#endif
    new (h) work_item_struct();
    return h;
}

void free_work_item(work_item_struct* h) {
#ifdef __NVCC__
    if(cap < 6)
        cudaFreeHost(h);
    else
        cudaFree(h);
#else
    free(h);
#endif
}

double work_item_cpu_cost_in_us = 0.0,
       work_item_gpu_cost_in_us = 0.0;

#define DEBUG_PRINT(x) std::cout << #x " = " << x << std::endl

double report(work_item_struct const& work_item_state, time_record tr, char const* lname, char const* sname, 
              uint32_t c_threads, uint32_t g_threads, bool contended, uint64_t cpu_work_item_count, uint64_t gpu_work_item_count, bool barrier) {

  auto const cpu_mean = c_threads ? std::accumulate(work_item_state.cpu_count, work_item_state.cpu_count + c_threads, 0ull) / c_threads : 0;
  auto const gpu_mean = g_threads ? std::accumulate(work_item_state.gpu_count, work_item_state.gpu_count + g_threads, 0ull) / g_threads : 0;
  auto const cpu_mean_sections = cpu_mean / cpu_work_item_count;
  auto const gpu_mean_sections = gpu_mean / gpu_work_item_count;
  auto const total_mean_sections = (std::accumulate(work_item_state.cpu_count, work_item_state.cpu_count + c_threads, 0ull) / cpu_work_item_count 
                                    + std::accumulate(work_item_state.gpu_count, work_item_state.gpu_count + g_threads, 0ull) / gpu_work_item_count) 
                                 / (c_threads + g_threads);

  auto const max_v = (std::numeric_limits<uint64_t>::max)();
  auto const min_f = [](uint64_t a, uint64_t b){ return (std::min)(a,b); };
  auto const cpu_min = std::accumulate(work_item_state.cpu_count, work_item_state.cpu_count + c_threads, max_v, min_f);
  auto const gpu_min = std::accumulate(work_item_state.gpu_count, work_item_state.gpu_count + g_threads, max_v, min_f);
  auto const cpu_min_sections = cpu_min / cpu_work_item_count;
  auto const gpu_min_sections = gpu_min / gpu_work_item_count;
  auto const total_min_sections = min_f(cpu_min_sections, gpu_min_sections);

  auto const min_v = (std::numeric_limits<uint64_t>::min)();
  auto const max_f = [](uint64_t a, uint64_t b){ return (std::max)(a,b); };
  auto const cpu_max = std::accumulate(work_item_state.cpu_count, work_item_state.cpu_count + c_threads, min_v, max_f);
  auto const gpu_max = std::accumulate(work_item_state.gpu_count, work_item_state.gpu_count + g_threads, min_v, max_f);
  auto const cpu_max_sections = cpu_max / cpu_work_item_count;
  auto const gpu_max_sections = gpu_max / gpu_work_item_count;
  auto const total_max_sections = max_f(cpu_max_sections, gpu_max_sections);

  auto const cpu_count = std::accumulate(work_item_state.cpu_count, work_item_state.cpu_count + c_threads, 0ull);
  auto const gpu_count = std::accumulate(work_item_state.gpu_count, work_item_state.gpu_count + g_threads, 0ull);
  auto const cpu_sum_sections = cpu_count / cpu_work_item_count;
  auto const gpu_sum_sections = gpu_count / gpu_work_item_count;
  auto const total_sum_sections = cpu_sum_sections + gpu_sum_sections;
  auto const cpu_avg_sections = c_threads ? std::ceil(1.0 * cpu_sum_sections / c_threads) : 0;
  auto const gpu_avg_sections = g_threads ? std::ceil(1.0 * gpu_sum_sections / g_threads) : 0;
  auto const total_avg_sections = std::ceil(1.0 * total_sum_sections / (c_threads + g_threads));

  auto cpct = c_threads ? 100.0 * cpu_min_sections / cpu_mean_sections : 100.0;
  auto gpct = g_threads ? 100.0 * gpu_min_sections / gpu_mean_sections : 100.0;
  auto fpct = 100.0 * total_min_sections / total_mean_sections;
  auto upct = 100.0*tr.user_time/(tr.user_time+tr.system_time);
  if(std::isnan(upct))
    upct = 100;

  auto const cpu_sections = barrier ? cpu_avg_sections : cpu_sum_sections;
  auto const gpu_sections = barrier ? gpu_avg_sections : gpu_sum_sections;
  auto const count = barrier ? (std::max)(cpu_avg_sections, gpu_avg_sections) : cpu_sections + gpu_sections;

  auto const rate = count * 1000000.0 / tr.us.count();
  auto const latency = (barrier || contended ? 1 : c_threads + g_threads) / rate;

  auto const sequential_time_in_us = cpu_sum_sections * cpu_work_item_count * work_item_cpu_cost_in_us + 
                                     gpu_sum_sections * gpu_work_item_count * work_item_gpu_cost_in_us;
  auto const parallel_time_in_us = max_f(uint64_t(cpu_avg_sections * cpu_work_item_count * work_item_cpu_cost_in_us),
                                         uint64_t(gpu_avg_sections * gpu_work_item_count * work_item_gpu_cost_in_us));
  auto const theory_time_in_us = (contended ? sequential_time_in_us : parallel_time_in_us);

  auto const overhead = tr.us.count() / theory_time_in_us;

  std::cout << std::setprecision(2);
  std::cout << std::setw(25) << lname
            << std::setw(15) << sname
            << std::setw(5) << c_threads
            << std::setw(10) << std::scientific << double(cpu_sections)
            << std::setw(10) << std::scientific << double(cpu_count)
            << std::setw(4) << int(cpct) << '%'
            << std::setw(10) << g_threads
            << std::setw(10) << std::scientific << double(gpu_sections)
            << std::setw(10) << std::scientific << double(gpu_count)
            << std::setw(4) << int(gpct) << '%'
            << std::setw(9) << std::fixed << tr.us.count()/1E6 << 's'
            << std::setw(9) << std::fixed << tr.user_time << 's'
            << std::setw(4) << int(upct) << '%'
            << std::setw(10) << std::scientific << rate 
            << std::setw(10) << std::scientific << latency 
            << std::setw(9) << std::fixed << overhead << 'x' 
            << std::setw(4) << int(fpct) << '%' << std::endl;
  csv << std::scientific << std::setprecision(3);
  csv << lname << ','
      << sname << ','
      << c_threads << ','
      << cpu_sections << ','
      << cpu_count << ','
      << cpct << ','
      << g_threads << ','
      << gpu_sections << ','
      << gpu_count << ','
      << gpct << ','
      << tr.us.count()/1E6 << ','
      << tr.user_time << ','
      << upct << ','
      << rate << ','
      << latency << ','
      << overhead << ','
      << fpct << std::endl;

    return overhead;
}

template<class F>
time_record run_core(work_item_struct& work_item_state, F f, uint32_t cthreads, uint32_t gthreads) {

    assert(cthreads <= MAX_CPU_THREADS);
    assert(gthreads <= MAX_GPU_THREADS);

    uint32_t const basetime = 3; // seconds, will get used for the real timer and the kill timer

    std::atomic<bool> kill{ true }, killed{ false };
    std::thread killer([&]() {
        for (int i = 0; i < 100; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2*100*basetime));
            if (!kill.load())
                return;
        }
        work_item_state.gpu_keep_going = 0;
        work_item_state.cpu_keep_going = 0;
        killed = true;
    });

    thread* const threads = (thread*)malloc(sizeof(thread)*cthreads);

    work_item_state.gpu_keep_going = 2;
    work_item_state.cpu_keep_going = 2;

#ifdef __NVCC__
    if(cap >= 6)
      cudaMemAdvise(&work_item_state, sizeof(work_item_state), cudaMemAdviseSetPreferredLocation, 0);
#endif

    auto const start = std::make_pair(std::chrono::steady_clock::now(), get_cpu_time());
    atomic_signal_fence(std::memory_order_seq_cst);
    for (uint32_t c = 0; c < cthreads; ++c)
      new (&threads[c]) thread([=]() {
        set_affinity(c);
#ifndef _MSC_VER
        #pragma nounroll
#endif
        while(f(c, true))
          ;
      });

    auto a = start_gpu_threads(gthreads, [=] __test_abi(uint32_t c) {
#ifndef _MSC_VER
    #pragma nounroll
#endif
      while(f(c, false))
        ;
    });

    std::this_thread::sleep_for(std::chrono::seconds(basetime));

    work_item_state.gpu_keep_going = 1;
    work_item_state.cpu_keep_going = 1;
    for (uint32_t c = 0; c < cthreads; ++c)
        threads[c].join();
    stop_gpu_threads(a);

    kill = false;
    killer.join();
    if (killed)
        std::cout << "KILLED" << std::endl;

    atomic_signal_fence(std::memory_order_seq_cst);
    auto const end = std::make_pair(std::chrono::steady_clock::now(), get_cpu_time());

    free(threads);

    return time_record{ std::chrono::duration_cast<std::chrono::microseconds>(end.first - start.first),
                        user_time_consumed(start.second, end.second), 
                        system_time_consumed(start.second, end.second) };
}

std::string onlyscenario;

static const std::string calibration_s = "-calibration-";

template<class F>
void run_scenario(F f, uint32_t& count, double& product, uint64_t target_duration_in_us, char const* lockname, char const* scenarioname, uint32_t cthreads, uint32_t gthreads, bool contended) {

  if(lockname != calibration_s && !onlyscenario.empty() && onlyscenario != scenarioname)
    return;

  auto const gpu_work_item_count = work_item_gpu_cost_in_us ? uint64_t(gthreads && target_duration_in_us ? std::ceil(target_duration_in_us / work_item_gpu_cost_in_us) : 1) : 10000;
  auto const target_cpu_duration = gthreads ? gpu_work_item_count * work_item_gpu_cost_in_us : target_duration_in_us;
  auto const cpu_work_item_count = work_item_cpu_cost_in_us ? uint64_t(cthreads && target_cpu_duration ? std::ceil(target_cpu_duration / work_item_cpu_cost_in_us) : 1) : 10000;

  work_item_struct* const wi = allocate_work_item();

  auto g = [=] __test_abi (uint32_t index, bool is_cpu) -> bool {
    auto const work_item_count = is_cpu ? cpu_work_item_count : gpu_work_item_count;
    auto const t = f(index, is_cpu);
    t->lock(); 
    int keep_going_state = 2;
#ifndef _MSC_VER
    #pragma nounroll
#endif
    for (uint64_t j = 0; j < work_item_count && keep_going_state > 0; ++j)
        keep_going_state = wi->do_it(index, is_cpu);
    t->unlock();
    return keep_going_state > 1;
  };
  auto const tr = run_core(*wi, g, cthreads, gthreads);

  if(work_item_cpu_cost_in_us == 0.0 && cthreads)
    work_item_cpu_cost_in_us = tr.us.count() / double(std::accumulate(wi->cpu_count, wi->cpu_count + cthreads, 0ull));
  if(work_item_gpu_cost_in_us == 0.0 && gthreads)
    work_item_gpu_cost_in_us = tr.us.count() / double(std::accumulate(wi->gpu_count, wi->gpu_count + gthreads, 0ull));

  product *= report(*wi, tr, lockname, scenarioname, cthreads, gthreads, contended, cpu_work_item_count, gpu_work_item_count, false);
  count += 1;

  free_work_item(wi);
}

void* allocate_heap(size_t s) {
    void* h;
    #ifdef __NVCC__
        if(cap < 6)
            cudaHostAlloc(&h, s, 0);
        else {
            cudaMallocManaged(&h, s);
            cudaMemAdvise(h, s, cudaMemAdviseSetPreferredLocation, 0);
        }
    #else
        h = malloc(s);
    #endif
    memset(h, 0, s);
    return h;
}

void free_heap(void* h) {
#ifdef __NVCC__
    if(cap < 6)
        cudaFreeHost(h);
    else
        cudaFree(h);
#else
    free(h);
#endif
}

template<class T>
void run_scenario_singlethreaded(char const* lockname, uint32_t& count, double& product, uint32_t cthreads, uint32_t gthreads) {

    void* heap = allocate_heap(sizeof(T)+alignof(T));
    T* t = new (heap) T;

    auto f = [=] __test_abi (uint32_t, bool) -> T* {
        return t;
    };
    run_scenario(f, count, product, 0, lockname, "singlethreaded", cthreads, gthreads, false);

    t->~T();
    free_heap(heap);
}

static constexpr int uncontended_count = 1<<20;

template<class T>
void run_scenario_uncontended(char const* lockname, uint32_t& count, double& product, uint32_t cthreads, uint32_t gthreads) {

    void* heap = allocate_heap(uncontended_count*sizeof(T) + alignof(T));
    T* t = new (heap) T[uncontended_count];

    assert(cthreads + gthreads <= uncontended_count);
    auto f = [=] __test_abi (uint32_t index, bool is_cpu) -> T* {
        if (is_cpu)
            return t + index;
        else
            return t + uncontended_count - 1 - index;
    };

    run_scenario(f, count, product, 0, lockname, "uncontended", cthreads, gthreads, false);

    for(size_t i = 0; i < uncontended_count; ++i)
        t[i].~T();
    free_heap(heap);
}

template<class T>
void run_scenario_shortest(char const* lockname, uint32_t& count, double& product, uint32_t cthreads, uint32_t gthreads) {

    void* heap = allocate_heap(sizeof(T) + alignof(T));
    T* t = new (heap) T;

    auto f = [=] __test_abi (uint32_t, bool) -> T* {
        return t;
    };
    run_scenario(f, count, product, 0, lockname, "shortest", cthreads, gthreads, true);

    t->~T();
    free_heap(heap);
}

template<class T>
void run_scenario_short(char const* lockname, uint32_t& count, double& product, uint32_t cthreads, uint32_t gthreads) {

    void* heap = allocate_heap(sizeof(T) + alignof(T));
    T* t = new (heap) T;

    auto f = [=] __test_abi (uint32_t, bool) -> T* {
        return t;
    };
    run_scenario(f, count, product, 1, lockname, "short", cthreads, gthreads, true);

    t->~T();
    free_heap(heap);
}

template<class T>
void run_scenario_long(char const* lockname, uint32_t& count, double& product, uint32_t cthreads, uint32_t gthreads) {
  
    void* heap = allocate_heap(sizeof(T) + alignof(T));
    T* t = new (heap) T;

    auto f = [=] __test_abi (uint32_t, bool) -> T* {
        return t;
    };
    run_scenario(f, count, product, 100, lockname, "long", cthreads, gthreads, true);

    t->~T();
    free_heap(heap);
}

template<class T>
void run_scenario_phaser(char const* lockname, char const* scenarioname, uint32_t wicount, uint32_t& count, double& product, uint32_t cthreads, uint32_t gthreads) {

    if (!onlyscenario.empty() && onlyscenario != scenarioname)
        return;

    void* heap = allocate_heap(sizeof(T) + alignof(T));
    T* t = new (heap) T(cthreads + gthreads);

    work_item_struct* const wi = allocate_work_item();

    auto g = [=] __test_abi(uint32_t index, bool is_cpu) -> bool {
        auto ret = wi->do_it(index, is_cpu) > 1;
        for(uint32_t i = 0;ret && i < wicount; ++i) {
            t->arrive_and_wait();
            ret = wi->do_it(index, is_cpu) > 1;
        }
        if(!ret)
            t->arrive_and_drop();
        return ret;
    };
    auto const tr = run_core(*wi, g, cthreads, gthreads);
    product *= report(*wi, tr, lockname, scenarioname, cthreads, gthreads, false, wicount, wicount, true);
    count += 1;

    free_work_item(wi);
    t->~T();
    free_heap(heap);
}

uint32_t onlycpu = ~0u, onlygpu = ~0u;

std::string onlylock;

template<class F>
void run_scenarios_inner(char const* lockname, F f) {

    if (lockname != calibration_s && !onlylock.empty() && onlylock != lockname)
        return;

    uint32_t const hc = std::thread::hardware_concurrency();
    uint32_t const cthreads[] = { 0, 1, hc >> 2, hc >> 1, (hc >> 2) + (hc >> 1), hc };
    uint32_t const maxg = max_gpu_threads();
    uint32_t const ming = maxg ? 1 : 0;
    uint32_t const lowg = maxg >> (cap >= 7 ? 5+4 : 4);
    uint32_t const midg = maxg >> (cap >= 7 ? 5 : 4);
    uint32_t const gthreads[] = { 0, ming, lowg >> 1, lowg, midg, maxg };

    std::set<std::pair<uint32_t, uint32_t>> s;
    for (uint32_t c : cthreads) {
        for (uint32_t g : gthreads) {
            if (lockname != calibration_s) {
                if (onlycpu != ~0u) c = onlycpu;
                if (onlygpu != ~0u) g = onlygpu;
            }
            auto const p = std::make_pair(c, g);
            if (s.find(p) != s.end())
                continue;
            s.insert(p);
            f(c, g);
        }
    }
}

template<class T>
void run_barrier_scenarios(char const* lockname, uint32_t& count, double& product) {
    return run_scenarios_inner(lockname, [&](int c, int g) {
        switch (c + g) {
        default:
            run_scenario_phaser<T>(lockname, "short_phaser", 1, count, product, c, g);
            run_scenario_phaser<T>(lockname, "long_phaser", 1000, count, product, c, g);
        case 0:
            ;
        }
    });
}
    
template<class T>
void run_mutex_scenarios(char const* lockname, uint32_t& count, double& product) {

    return run_scenarios_inner(lockname, [&](int c, int g) {
        switch (c + g) {
        case 1:
            run_scenario_singlethreaded<T>(lockname, count, product, c, g);
        case 0:
            break;
        default:
            run_scenario_uncontended<T>(lockname, count, product, c, g);
            run_scenario_shortest<T>(lockname, count, product, c, g);
            run_scenario_short<T>(lockname, count, product, c, g);
            run_scenario_long<T>(lockname, count, product, c, g);
        }
    });
}

template<class Fn>
void run_and_report_scenarios(Fn fn, char const* lockname, uint32_t& count, double& product) {

    uint32_t count_ = 0; 
    double product_ = 1.0;

    fn(lockname, count_, product_);
    if(count_)
      std::cout << "== " << lockname << " : " << std::fixed << std::setprecision(0) << 10000/std::pow(product_, 1.0/count_) << " lockmarks ==" << std::endl;
    
    count += count_;
    product *= product_;
}

#define run_and_report_mutex_scenarios(x,c,s) run_and_report_scenarios(run_mutex_scenarios<wrapped_mutex<x>>, #x,c,s)
#define run_and_report_barrier_scenarios(x,c,s) run_and_report_scenarios(run_barrier_scenarios<x>,#x,c,s)

struct null_mutex {

  __test_abi void lock() { }
  __test_abi void unlock() { }

};

void run_calibration() {

  uint32_t count = 0;
  double product = 1.0;

  return run_scenarios_inner(calibration_s.c_str(), [&](int c, int g) {
    switch (c + g) {
      case 1:
        run_scenario_singlethreaded<null_mutex>(calibration_s.c_str(), count, product, c, g);
      default:
        ;
    }
  });
}

#ifdef __test_wtf

    #include "config.h"

    #include "ToyLocks.h"
    #include <thread>
    #include <unistd.h>
    #include <wtf/CurrentTime.h>
    #include <wtf/DataLog.h>
    #include <wtf/HashMap.h>
    #include <wtf/Lock.h>
    #include <wtf/ParkingLot.h>
    #include <wtf/StdLibExtras.h>
    #include <wtf/Threading.h>
    #include <wtf/ThreadingPrimitives.h>
    #include <wtf/Vector.h>
    #include <wtf/WordLock.h>
    #include <wtf/text/CString.h>

#endif

template<class M>
struct alignas(64) wrapped_mutex {

    wrapped_mutex() : __m() { }
    wrapped_mutex(wrapped_mutex const&) = delete;
    wrapped_mutex& operator=(wrapped_mutex const&) = delete;

    __test_abi void lock() noexcept {
        __m.lock();
    }
    __test_abi void unlock() noexcept {
        __m.unlock();
    }

    M __m;
};

void print_headers() {

  std::cout << std::setw(25) << "lock"
            << std::setw(15) << "scenario"
            << std::setw(5) << "cpu" 
            << std::setw(10) << "sections"
            << std::setw(10) << "items"
            << std::setw(5) << "fair"
            << std::setw(10) << "gpu"
            << std::setw(10) << "sections"
            << std::setw(10) << "items"
            << std::setw(5) << "fair"
            << std::setw(10) << "walltime"
            << std::setw(10) << "cputime"
            << std::setw(5)  << "u:s"
            << std::setw(10) << "rate" 
            << std::setw(10) << "latency" 
            << std::setw(10) << "overhead" 
            << std::setw(5) << "fair"
            << std::endl;

  csv << "lock,"
      << "scenario,"
      << "cpu,"
      << "sections,"
      << "items,"
      << "fair,"
      << "gpu,"
      << "sections,"
      << "items,"
      << "fair,"
      << "walltime,"
      << "cputime,"
      << "u:s,"
      << "rate,"
      << "latency,"
      << "overhead,"
      << "fair,"
      << std::endl;
}
