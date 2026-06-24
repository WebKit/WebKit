/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Mirrors Runner/TestWebKitAPI.swift (the Xcode @main). Drop and switch to the
// Swift runner once the CMake port supports Swift + Swift Testing.

#import "config.h"

#import "Runner/TestsController.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import <string>
#import <vector>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace {

void resetPersistentDomain()
{
    [[NSUserDefaults standardUserDefaults] removePersistentDomainForName:@"TestWebKitAPI"];
}

NSMutableDictionary *mutableArgumentDomain()
{
    NSDictionary *current = [[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain];
    if (current)
        return [current mutableCopy];
    return [NSMutableDictionary dictionary];
}

void writeArgumentDomain(NSDictionary *domain)
{
    [[NSUserDefaults standardUserDefaults] setVolatileDomain:domain forName:NSArgumentDomain];
}

void applyAlwaysSetDefaults()
{
    NSMutableDictionary *args = mutableArgumentDomain();
    // FIXME: Switch to overlay scrollbars (the platform default) once tests are updated.
    args[@"NSOverlayScrollersEnabled"] = @NO;
    args[@"AppleShowScrollBars"] = @"Always";
    // FIXME: Remove once rdar://159372811 is fixed.
    args[@"NSEventConcurrentProcessingEnabled"] = @NO;
    writeArgumentDomain(args);
}

void setBoolArgumentDomain(NSString *key, BOOL value)
{
    NSMutableDictionary *args = mutableArgumentDomain();
    args[key] = @(value);
    writeArgumentDomain(args);
}

void forceSiteIsolation()
{
    Class cls = NSClassFromString(@"WKPreferences");
    SEL sel = NSSelectorFromString(@"_forceSiteIsolationAlwaysOnForTesting");
    if (!cls || ![cls respondsToSelector:sel])
        return;
    // performSelector returns void; the leak warning is spurious.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [(id)cls performSelector:sel];
#pragma clang diagnostic pop
}

std::vector<std::string> translateArgv(int argc, char** argv)
{
    std::vector<std::string> out;
    out.reserve(argc + 8);

    if (argc > 0)
        out.emplace_back(argv[0]);

    std::vector<std::string> posFilters;
    std::vector<std::string> negFilters;
    bool listTests = false;
    bool force = false;
    bool pretty = false;
    bool hasRepetitions = false;
    std::string repetitions;

    auto needsValue = [&](int& i, const char* /*flag*/) -> const char* {
        if (i + 1 >= argc)
            return nullptr;
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        std::string token { argv[i] };

        if (token == "--filter") {
            if (const char* value = needsValue(i, "--filter"))
                posFilters.emplace_back(value);
            continue;
        }
        if (token == "--skip") {
            if (const char* value = needsValue(i, "--skip"))
                negFilters.emplace_back(value);
            continue;
        }
        if (token == "--repetitions") {
            if (const char* value = needsValue(i, "--repetitions")) {
                repetitions = value;
                hasRepetitions = true;
            }
            continue;
        }
        if (token == "--list-tests") {
            listTests = true;
            continue;
        }
        if (token == "--force") {
            force = true;
            continue;
        }
        if (token == "--pretty") {
            pretty = true;
            continue;
        }

        if (token == "--site-isolation-enabled-by-default") {
            forceSiteIsolation();
            continue;
        }
        if (token == "--remote-layer-tree") {
            setBoolArgumentDomain(@"WebKit2UseRemoteLayerTreeDrawingArea", YES);
            continue;
        }
        if (token == "--no-remote-layer-tree") {
            setBoolArgumentDomain(@"WebKit2UseRemoteLayerTreeDrawingArea", NO);
            continue;
        }
        if (token == "--use-gpu-process") {
            setBoolArgumentDomain(@"WebKit2GPUProcessForDOMRendering", YES);
            continue;
        }
        if (token == "--no-use-gpu-process") {
            setBoolArgumentDomain(@"WebKit2GPUProcessForDOMRendering", NO);
            continue;
        }
        if (token == "--parallel")
            continue;

        out.push_back(WTF::move(token));
    }

    if (listTests)
        out.emplace_back("--gtest_list_tests");

    if (pretty)
        out.emplace_back("--gtest_brief=0");

    if (force)
        out.emplace_back("--gtest_also_run_disabled_tests");

    // gtest treats a leading '-' in --gtest_filter as "exclude only".
    if (!posFilters.empty() || !negFilters.empty()) {
        std::string filter = "--gtest_filter=";
        for (size_t i = 0; i < posFilters.size(); ++i) {
            if (i)
                filter += ':';
            filter += posFilters[i];
        }
        if (!negFilters.empty()) {
            filter += '-';
            for (size_t i = 0; i < negFilters.size(); ++i) {
                if (i)
                    filter += ':';
                filter += negFilters[i];
            }
        }
        out.push_back(WTF::move(filter));
    }

    if (hasRepetitions)
        out.emplace_back("--gtest_repeat=" + repetitions);

    return out;
}

} // namespace

int main(int argc, char** argv)
{
    @autoreleasepool {
        WTF::enableAllSDKAlignedBehaviors();
        resetPersistentDomain();
        applyAlwaysSetDefaults();

        auto translated = translateArgv(argc, argv);

        std::vector<char*> ptrs;
        ptrs.reserve(translated.size() + 1);
        for (auto& s : translated)
            ptrs.push_back(s.data());
        ptrs.push_back(nullptr);
        int newArgc = static_cast<int>(translated.size());

        // Instantiate NSApp; some tests rely on it being non-nil.
        (void)[NSApplication sharedApplication];

        return TestWebKitAPI::TestsController::singleton().run(newArgc, ptrs.data()) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
