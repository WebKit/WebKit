/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <inttypes.h>
#include <optional>
#include <stdio.h>
#include <sys/time.h>
#include <vector>
#include <wtf/MonotonicTime.h>

#if PLATFORM(PLAYSTATION)
#include <showmap.h>
#endif

static void description()
{
    printf("usage \n testmem <path-to-file-to-run> [iterations]\n");
}

struct Footprint {
    uint64_t current;
    uint64_t peak;

    static std::optional<Footprint> now()
    {
#if PLATFORM(PLAYSTATION)
        showmap::Result result;
        result.collect();
        if (auto* entry = result.entry("SceNKFastMalloc")) {
            return Footprint {
                static_cast<uint64_t>(entry->effectiveRss()),
                static_cast<uint64_t>(entry->vss)
            };
        }
#else
#error "No testmem implementation for this platform."
#endif
        return std::nullopt;
    }
};

static JSStringRef readScript(const char* path)
{
    FILE* file = fopen(path, "r");
    if (!file)
        return nullptr;

    fseek(file, 0, SEEK_END);
    auto length = ftell(file);
    fseek(file, 0, SEEK_SET);

    std::vector<char> buffer(length + 1);
    fread(buffer.data(), length, 1, file);
    buffer[length] = 0;
    fclose(file);

    return JSStringCreateWithUTF8CString(buffer.data());
}

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

    const char* path = argv[1];
    auto script = readScript(path);
    if (!script) {
        printf("Can't open file: %s\n", path);
        exit(1);
    }

    auto sourceURL = JSStringCreateWithUTF8CString(path);

    auto startTime = MonotonicTime::now();
    JSContextGroupRef group = JSContextGroupCreate();
    for (size_t i = 0; i < iterations; ++i) {
        JSGlobalContextRef context = JSGlobalContextCreateInGroup(group, nullptr);
        JSValueRef exception = nullptr;
        JSEvaluateScript(context, script, nullptr, sourceURL, 0, &exception);
        if (exception) {
            printf("Unexpected exception thrown\n");
            exit(1);
        }
        JSGlobalContextRelease(context);
    }

    auto time = MonotonicTime::now() - startTime;
    if (auto footprint = Footprint::now()) {
        printf("time: %lf\n", time.seconds()); // Seconds
        printf("peak footprint: %" PRIu64 "\n", footprint->peak); // Bytes
        printf("footprint at end: %" PRIu64 "\n", footprint->current); // Bytes
    } else {
        printf("Failure when calling rusage\n");
        exit(1);
    }

    return 0;
}
