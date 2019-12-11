/*
 * Copyright 2011-2015 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CK_COMMON_H
#define CK_COMMON_H

#include <ck_cc.h>
#include <ck_pr.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef __linux__
#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>
#elif defined(__MACH__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/cpuset.h>
#endif

#if defined(_WIN32)
#include <assert.h>
#define NOMINMAX
#include <windows.h>
#define DELTA_EPOCH  11644473600000000ULL
#else
#include <signal.h>
#include <unistd.h>
#endif

#ifndef CORES
#define CORES 8
#endif

CK_CC_INLINE static void
common_srand(unsigned int i)
{
#ifdef _WIN32
	srand(i);
#else
	srandom(i);
#endif
}

CK_CC_INLINE static int
common_rand(void)
{
#ifdef _WIN32
	return rand();
#else
	return random();
#endif
}

CK_CC_INLINE static int
common_rand_r(unsigned int *i)
{
#ifdef _WIN32
	(void)i;

	/*
	 * When linked with -mthreads, rand() is thread-safe.
	 * rand_s is also an option.
	 */
	return rand();
#else
	return rand_r(i);
#endif
}

CK_CC_INLINE static void
common_srand48(long int i)
{
#ifdef _WIN32
	srand(i);
#else
	srand48(i);
#endif
}

CK_CC_INLINE static long int
common_lrand48(void)
{
#ifdef _WIN32
	return rand();
#else
	return lrand48();
#endif
}

CK_CC_INLINE static double
common_drand48(void)
{
#ifdef _WIN32
	return (double)rand()/RAND_MAX;
#else
	return drand48();
#endif
}

CK_CC_INLINE static void
common_sleep(unsigned int n)
{
#ifdef _WIN32
	Sleep(n * 1000);
#else
	sleep(n);
#endif
}

CK_CC_INLINE static int
common_gettimeofday(struct timeval *tv, void *tz)
{
#ifdef _WIN32
	FILETIME ft;
	uint64_t tmp_time = 0;
	static bool tzflag = false;
	struct timezone *tzp = tz;

	if (tv != NULL) {
		GetSystemTimeAsFileTime(&ft);
		tmp_time |= ft.dwHighDateTime;
		tmp_time <<= 32;
		tmp_time |= ft.dwLowDateTime;

		/* GetSystemTimeAsFileTime returns 100 nanosecond intervals. */
		tmp_time /= 10;

		/* Windows' epoch starts on 01/01/1601, while Unix' starts on 01/01/1970. */
		tmp_time -= DELTA_EPOCH;

		tv->tv_sec = (long)(tmp_time / 1000000UL);
		tv->tv_usec = (long)(tmp_time % 1000000UL);
	}


	if (tz != NULL) {
		if (tzflag == false) {
			_tzset();
			tzflag = true;
		}

		tzp->tz_minuteswest = _timezone / 60;
		tzp->tz_dsttime = _daylight;
	}

	return 0;
#else
	return gettimeofday(tv, tz);
#endif
}

CK_CC_UNUSED static unsigned int
common_alarm(void (*sig_handler)(int), void *alarm_event, unsigned int duration)
{
#ifdef _WIN32
	(void)sig_handler;
	(void)duration;
	bool success;
	HANDLE *alarm_handle = alarm_event;
	success = SetEvent(*alarm_handle);
	assert(success != false);
	return 0;
#else
	(void)alarm_event;
	signal(SIGALRM, sig_handler);
	return alarm(duration);
#endif
}

#ifdef _WIN32
#ifndef SECOND_TIMER
#define	SECOND_TIMER 10000000
#endif
#define	COMMON_ALARM_DECLARE_GLOBAL(prefix, alarm_event_name, flag_name)				\
static HANDLE prefix##_common_win_alarm_timer;								\
static HANDLE alarm_event_name;										\
static LARGE_INTEGER prefix##_common_alarm_timer_length;						\
													\
static void CALLBACK											\
prefix##_common_win_alarm_handler(LPVOID arg, DWORD timer_low_value, DWORD timer_high_value)		\
{													\
	(void)arg;											\
	(void)timer_low_value;										\
	(void)timer_high_value;										\
	flag_name = true;										\
	return;												\
}													\
													\
static void *												\
prefix##_common_win_alarm(void *unused)									\
{													\
	(void)unused;											\
	bool timer_success = false;									\
	for (;;) {											\
		WaitForSingleObjectEx(alarm_event_name, INFINITE, true);				\
		timer_success = SetWaitableTimer(prefix##_common_win_alarm_timer,			\
						 &prefix##_common_alarm_timer_length,			\
						 0,							\
						 prefix##_common_win_alarm_handler, NULL, false);	\
		assert(timer_success != false);								\
		WaitForSingleObjectEx(prefix##_common_win_alarm_timer, INFINITE, true);			\
	}												\
													\
	return NULL;											\
}

#define	COMMON_ALARM_DECLARE_LOCAL(prefix, alarm_event_name)	\
	int64_t prefix##_common_alarm_tl;			\
	pthread_t prefix##_common_win_alarm_thread;

#define	COMMON_ALARM_INIT(prefix, alarm_event_name, duration) 				\
	prefix##_common_alarm_tl = -1 * (duration) * SECOND_TIMER;			\
	prefix##_common_alarm_timer_length.LowPart =					\
		(DWORD) (prefix##_common_alarm_tl & 0xFFFFFFFF);			\
	prefix##_common_alarm_timer_length.HighPart = 					\
		(LONG) (prefix##_common_alarm_tl >> 32);				\
	alarm_event_name = CreateEvent(NULL, false, false, NULL);			\
	assert(alarm_event_name != NULL);						\
	prefix##_common_win_alarm_timer = CreateWaitableTimer(NULL, true, NULL);	\
	assert(prefix##_common_win_alarm_timer != NULL);				\
	if (pthread_create(&prefix##_common_win_alarm_thread,				\
			   NULL,							\
			   prefix##_common_win_alarm,					\
			   NULL) != 0)							\
		ck_error("ERROR: Failed to create common_win_alarm thread.\n");
#else
#define	COMMON_ALARM_DECLARE_GLOBAL(prefix, alarm_event_name, flag_name)
#define	COMMON_ALARM_DECLARE_LOCAL(prefix, alarm_event_name)	\
	int alarm_event_name = 0;
#define	COMMON_ALARM_INIT(prefix, alarm_event_name, duration)
#endif

struct affinity {
	unsigned int delta;
	unsigned int request;
};

#define AFFINITY_INITIALIZER {0, 0}

#ifdef __linux__
#ifndef gettid
static pid_t
gettid(void)
{
	return syscall(__NR_gettid);
}
#endif /* gettid */

CK_CC_UNUSED static int
aff_iterate(struct affinity *acb)
{
	cpu_set_t s;
	unsigned int c;

	c = ck_pr_faa_uint(&acb->request, acb->delta);
	CPU_ZERO(&s);
	CPU_SET(c % CORES, &s);

	return sched_setaffinity(gettid(), sizeof(s), &s);
}

CK_CC_UNUSED static int
aff_iterate_core(struct affinity *acb, unsigned int *core)
{
	cpu_set_t s;

	*core = ck_pr_faa_uint(&acb->request, acb->delta);
	CPU_ZERO(&s);
	CPU_SET((*core) % CORES, &s);

	return sched_setaffinity(gettid(), sizeof(s), &s);
}
#elif defined(__MACH__)
CK_CC_UNUSED static int
aff_iterate(struct affinity *acb)
{
	thread_affinity_policy_data_t policy;
	unsigned int c;

	c = ck_pr_faa_uint(&acb->request, acb->delta) % CORES;
	policy.affinity_tag = c;
	return thread_policy_set(mach_thread_self(),
				 THREAD_AFFINITY_POLICY,
				 (thread_policy_t)&policy,
				 THREAD_AFFINITY_POLICY_COUNT);
}

CK_CC_UNUSED static int
aff_iterate_core(struct affinity *acb, unsigned int *core)
{
	thread_affinity_policy_data_t policy;

	*core = ck_pr_faa_uint(&acb->request, acb->delta) % CORES;
	policy.affinity_tag = *core;
	return thread_policy_set(mach_thread_self(),
				 THREAD_AFFINITY_POLICY,
				 (thread_policy_t)&policy,
				 THREAD_AFFINITY_POLICY_COUNT);
}
#elif defined(__FreeBSD__)
CK_CC_UNUSED static int
aff_iterate(struct affinity *acb CK_CC_UNUSED)
{
	unsigned int c;
	cpuset_t mask;

	c = ck_pr_faa_uint(&acb->request, acb->delta) % CORES;
	CPU_ZERO(&mask);
	CPU_SET(c, &mask);
	return (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask));
}

CK_CC_UNUSED static int
aff_iterate_core(struct affinity *acb CK_CC_UNUSED, unsigned int *core)
{
	cpuset_t mask;

	*core = ck_pr_faa_uint(&acb->request, acb->delta) % CORES;
	CPU_ZERO(&mask);
	CPU_SET(*core, &mask);
	return (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask));
}
#else
CK_CC_UNUSED static int
aff_iterate(struct affinity *acb CK_CC_UNUSED)
{

	return (0);
}

CK_CC_UNUSED static int
aff_iterate_core(struct affinity *acb CK_CC_UNUSED, unsigned int *core)
{
	*core = 0;
	return (0);
}
#endif

CK_CC_INLINE static uint64_t
rdtsc(void)
{
#if defined(__x86_64__)
	uint32_t eax = 0, edx;
#if defined(CK_MD_RDTSCP)
	__asm__ __volatile__("rdtscp"
				: "+a" (eax), "=d" (edx)
				:
				: "%ecx", "memory");

	return (((uint64_t)edx << 32) | eax);
#else
        __asm__ __volatile__("cpuid;"
                             "rdtsc;"
                                : "+a" (eax), "=d" (edx)
                                :
                                : "%ebx", "%ecx", "memory");

        __asm__ __volatile__("xorl %%eax, %%eax;"
                             "cpuid;"
                                :
                                :
                                : "%eax", "%ebx", "%ecx", "%edx", "memory");

        return (((uint64_t)edx << 32) | eax);
#endif /* !CK_MD_RDTSCP */
#elif defined(__x86__)
	uint32_t eax = 0, edx;
#if defined(CK_MD_RDTSCP)
	__asm__ __volatile__("rdtscp"
				: "+a" (eax), "=d" (edx)
				:
				: "%ecx", "memory");

	return (((uint64_t)edx << 32) | eax);
#else
        __asm__ __volatile__("pushl %%ebx;"
			     "cpuid;"
                             "rdtsc;"
                                : "+a" (eax), "=d" (edx)
                                :
                                : "%ecx", "memory");

        __asm__ __volatile__("xorl %%eax, %%eax;"
                             "cpuid;"
			     "popl %%ebx;"
                                :
                                :
                                : "%eax", "%ecx", "%edx", "memory");

        return (((uint64_t)edx << 32) | eax);
#endif /* !CK_MD_RDTSCP */
#elif defined(__sparcv9__)
	uint64_t r;

        __asm__ __volatile__("rd %%tick, %0"
				: "=r" (r)
				:
				: "memory");
        return r;
#elif defined(__ppc64__)
	uint32_t high, low, snapshot;

	do {
	  __asm__ __volatile__("isync;"
			       "mftbu %0;"
			       "mftb  %1;"
			       "mftbu %2;"
				: "=r" (high), "=r" (low), "=r" (snapshot)
				:
				: "memory");
	} while (snapshot != high);

	return (((uint64_t)high << 32) | low);
#elif defined(__aarch64__)
	uint64_t r;

	__asm __volatile__ ("mrs %0, cntvct_el0" : "=r" (r) : : "memory");
	return r;
#else
	return 0;
#endif
}

CK_CC_USED static void
ck_error(const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vfprintf(stderr, message, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

#define ck_test(A, B, ...) do {			\
	if (A)					\
		ck_error(B, ##__VA_ARGS__);	\
} while (0)

#endif /* CK_COMMON_H */
