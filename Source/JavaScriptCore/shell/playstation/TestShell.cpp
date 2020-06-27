/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../jsc.cpp"

#include <wtf/Language.h>

#define STATIC_OPTION(type_, name_, defaultValue_, availability_, description_) \
    static OptionsStorage::type_ orig##name_;
    FOR_EACH_JSC_OPTION(STATIC_OPTION)
#undef STATIC_OPTION

extern "C" void setupTestRun()
{
    CommandLine options(0, nullptr);
#define STATIC_OPTION(type_, name_, defaultValue_, availability_, description_) \
    orig##name_ = JSC::Options::name_();
    FOR_EACH_JSC_OPTION(STATIC_OPTION)
#undef STATIC_OPTION

    // Need to initialize WTF threading before we start any threads. Cannot initialize JSC
    // threading yet, since that would do somethings that we'd like to defer until after we
    // have a chance to parse options.
    WTF::initializeMainThread();

    // Need to override and enable restricted options before we start parsing options below.
    Config::enableRestrictedOptions();

    JSC::initializeThreading();

#if ENABLE(WEBASSEMBLY)
    JSC::Wasm::enableFastMemory();
#endif
    Gigacage::forbidDisablingPrimitiveGigacage();
}

extern "C" void preTest()
{
#define INIT_OPTION(type_, name_, defaultValue_, availability_, description_) \
    JSC::Options::name_() = orig##name_;
    FOR_EACH_JSC_OPTION(INIT_OPTION)
#undef INIT_OPTION
    overrideUserPreferredLanguages(platformUserPreferredLanguages());
}

extern "C" int runTest(int argc, char* argv[])
{
    CommandLine options(argc, argv);
    processConfigFile(Options::configFile(), "jsc");

    return runJSC(
        options, true,
        [&](VM& vm, GlobalObject* globalObject, bool& success) {
            UNUSED_PARAM(vm);
            runWithOptions(globalObject, options, success);
        });
}

extern "C" void postTest()
{
}

extern "C" void shutdownTestRun()
{
}
