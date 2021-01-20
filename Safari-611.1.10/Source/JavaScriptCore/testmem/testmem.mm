/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <inttypes.h>
#import <stdio.h>

#if __has_include(<libproc.h>)
#define HAS_LIBPROC 1
#import <libproc.h>
#else
#define HAS_LIBPROC 0
#endif

#if HAS_LIBPROC && RUSAGE_INFO_CURRENT >= 4 && JSC_OBJC_API_ENABLED
static void description()
{
    printf("usage \n testmem <path-to-file-to-run> [iterations]\n");
}

struct Footprint {
    uint64_t current;
    uint64_t peak;

    static Footprint now()
    {
        rusage_info_v4 rusage;
        if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&rusage)) {
            printf("Failure when calling rusage\n");
            exit(1);
        }

        return { rusage.ri_phys_footprint, rusage.ri_lifetime_max_phys_footprint };
    }
};

int main(int argc, char* argv[])
{
    if (argc < 2) {
        description();
        exit(1);
    }

    size_t iterations = 20;
    if (argc >= 3) {
        int iters = atoi(argv[2]);
        if (iters < 0) {
            printf("Iterations argument must be >= 0");
            exit(1);
        }
        iterations = iters;
    }

    NSString *path = [NSString stringWithUTF8String:argv[1]];
    NSString *script = [[NSString alloc] initWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    if (!script) {
        printf("Can't open file: %s\n", argv[1]);
        exit(1);
    }

    auto startTime = CFAbsoluteTimeGetCurrent();
    JSVirtualMachine* vm = [[JSVirtualMachine alloc] init];
    for (size_t i = 0; i < iterations; ++i) {
        @autoreleasepool {
            JSContext *context = [[JSContext alloc] initWithVirtualMachine:vm];
            context.exceptionHandler = ^(JSContext*, JSValue*) {
                printf("Unexpected exception thrown\n");
                exit(1);
            };
            [context evaluateScript:script];
        }
    }
    auto time = CFAbsoluteTimeGetCurrent() - startTime;
    auto footprint = Footprint::now();

    printf("time: %lf\n", time); // Seconds
    printf("peak footprint: %" PRIu64 "\n", footprint.peak); // Bytes
    printf("footprint at end: %" PRIu64 "\n", footprint.current); // Bytes

    return 0;
}
#else
int main(int, char*[])
{
    printf("You need to compile this file with an SDK that has RUSAGE_INFO_V4 or later\n");
    return 1;
}
#endif
