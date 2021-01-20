/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#include "NetworkStateNotifier.h"

#if PLATFORM(MAC)

#include <SystemConfiguration/SystemConfiguration.h>

namespace WebCore {

void NetworkStateNotifier::updateStateWithoutNotifying()
{
    auto key = adoptCF(SCDynamicStoreKeyCreateNetworkInterface(0, kSCDynamicStoreDomainState));
    auto propertyList = adoptCF(SCDynamicStoreCopyValue(m_store.get(), key.get()));
    if (!propertyList || CFGetTypeID(propertyList.get()) != CFDictionaryGetTypeID())
        return;

    auto netInterfaces = CFDictionaryGetValue((CFDictionaryRef)propertyList.get(), kSCDynamicStorePropNetInterfaces);
    if (!netInterfaces || CFGetTypeID(netInterfaces) != CFArrayGetTypeID())
        return;

    for (CFIndex i = 0; i < CFArrayGetCount((CFArrayRef)netInterfaces); i++) {
        auto interfaceName = (CFStringRef)CFArrayGetValueAtIndex((CFArrayRef)netInterfaces, i);
        if (CFGetTypeID(interfaceName) != CFStringGetTypeID())
            continue;

        // Ignore the loopback interface.
        if (CFStringHasPrefix(interfaceName, CFSTR("lo")))
            continue;

        // Ignore Parallels virtual interfaces on host machine as these are always up.
        if (CFStringHasPrefix(interfaceName, CFSTR("vnic")))
            continue;

        // Ignore VMWare virtual interfaces on host machine as these are always up.
        if (CFStringHasPrefix(interfaceName, CFSTR("vmnet")))
            continue;

        auto key = adoptCF(SCDynamicStoreKeyCreateNetworkInterfaceEntity(0, kSCDynamicStoreDomainState, interfaceName, kSCEntNetIPv4));
        if (auto value = adoptCF(SCDynamicStoreCopyValue(m_store.get(), key.get()))) {
            m_isOnLine = true;
            return;
        }
    }

    m_isOnLine = false;
}

void NetworkStateNotifier::startObserving()
{
    SCDynamicStoreContext context = { 0, this, 0, 0, 0 };
    m_store = adoptCF(SCDynamicStoreCreate(0, CFSTR("com.apple.WebCore"), [] (SCDynamicStoreRef, CFArrayRef, void*) {
        // Calling updateState() could be expensive so we coalesce calls with a timer.
        singleton().updateStateSoon();
    }, &context));
    if (!m_store)
        return;

    auto source = adoptCF(SCDynamicStoreCreateRunLoopSource(0, m_store.get(), 0));
    if (!source)
        return;

    CFRunLoopAddSource(CFRunLoopGetMain(), source.get(), kCFRunLoopCommonModes);

    auto keys = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    CFArrayAppendValue(keys.get(), adoptCF(SCDynamicStoreKeyCreateNetworkGlobalEntity(0, kSCDynamicStoreDomainState, kSCEntNetIPv4)).get());
    CFArrayAppendValue(keys.get(), adoptCF(SCDynamicStoreKeyCreateNetworkGlobalEntity(0, kSCDynamicStoreDomainState, kSCEntNetDNS)).get());

    auto patterns = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    CFArrayAppendValue(patterns.get(), adoptCF(SCDynamicStoreKeyCreateNetworkInterfaceEntity(0, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4)).get());

    SCDynamicStoreSetNotificationKeys(m_store.get(), keys.get(), patterns.get());
}
    
}

#endif // PLATFORM(MAC)
