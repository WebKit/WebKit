/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebPageProxy.h"

#include "Logging.h"
#include "WebBackForwardList.h"
#include "WebData.h"

using namespace WebCore;

namespace WebKit {

DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryKey, (CFSTR("SessionHistory")));

static const UInt32 CurrentSessionStateDataVersion = 1;

PassRefPtr<WebData> WebPageProxy::sessionStateData(WebPageProxySessionStateFilterCallback filter, void* context) const
{
    RetainPtr<CFDictionaryRef> sessionHistoryDictionary(AdoptCF, m_backForwardList->createCFDictionaryRepresentation(filter, context));
    
    // For now we're only serializing the back/forward list.  If that object is null, then the entire sessionState can be null.
    if (!sessionHistoryDictionary)
        return 0;
    
    const void* keys[1] = { SessionHistoryKey() };
    const void* values[1] = { sessionHistoryDictionary.get() };
    
    RetainPtr<CFDictionaryRef> stateDictionary(AdoptCF, CFDictionaryCreate(0, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFDataRef> stateCFData(AdoptCF, CFPropertyListCreateData(0, stateDictionary.get(), kCFPropertyListBinaryFormat_v1_0, 0, 0));

    CFIndex length = CFDataGetLength(stateCFData.get());
    Vector<unsigned char> stateVector(length + 4);
    
    // Put the session state version number at the start of the buffer
    stateVector.data()[0] = (CurrentSessionStateDataVersion & 0xFF000000) >> 24;
    stateVector.data()[1] = (CurrentSessionStateDataVersion & 0x00FF0000) >> 16;
    stateVector.data()[2] = (CurrentSessionStateDataVersion & 0x0000FF00) >> 8;
    stateVector.data()[3] = (CurrentSessionStateDataVersion & 0x000000FF);
    
    // Copy in the actual session state data
    CFDataGetBytes(stateCFData.get(), CFRangeMake(0, length), stateVector.data() + 4);
    
    return WebData::create(stateVector);
}

void WebPageProxy::restoreFromSessionStateData(WebData* webData)
{
    if (!webData || webData->size() < 4)
        return;

    const unsigned char* buffer = webData->bytes();
    UInt32 versionHeader = (buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
    
    if (versionHeader != CurrentSessionStateDataVersion) {
        LOG(SessionState, "Unrecognized version header for session state data - cannot restore");
        return;
    }
    
    RetainPtr<CFDataRef> data(AdoptCF, CFDataCreate(0, webData->bytes() + 4, webData->size() - 4));

    CFErrorRef propertyListError = 0;
    RetainPtr<CFPropertyListRef> propertyList(AdoptCF, CFPropertyListCreateWithData(0, data.get(), kCFPropertyListImmutable, 0, &propertyListError));
    if (propertyListError) {
        LOG(SessionState, "Could not read session state property list");
        return;
    }

    if (!propertyList)
        return;
        
    if (CFGetTypeID(propertyList.get()) != CFDictionaryGetTypeID()) {
        LOG(SessionState, "SessionState property list is not a CFDictionaryRef (%i) - its CFTypeID is %i", (int)CFDictionaryGetTypeID(), (int)CFGetTypeID(propertyList.get()));
        return;
    }
    
    CFTypeRef sessionHistoryRef = CFDictionaryGetValue(static_cast<CFDictionaryRef>(propertyList.get()), SessionHistoryKey());
    if (!sessionHistoryRef || CFGetTypeID(sessionHistoryRef) != CFDictionaryGetTypeID()) {
        LOG(SessionState, "SessionState dictionary does not contain a SessionHistoryDictionary key, it is of the wrong type");
        return;
    }
    
    CFDictionaryRef sessionHistoryDictionary = static_cast<CFDictionaryRef>(sessionHistoryRef);
    if (!m_backForwardList->restoreFromCFDictionaryRepresentation(sessionHistoryDictionary)) {
        LOG(SessionState, "Failed to restore back/forward list from SessionHistoryDictionary");
        return;
    }

    // FIXME: When we have a solution for restoring the full back/forward list then causing a load of the current item,
    // we will trigger that load here.  Until then, we use the "restored current URL" which can later be removed.
    loadURL(m_backForwardList->restoredCurrentURL());
}

} // namespace WebKit
