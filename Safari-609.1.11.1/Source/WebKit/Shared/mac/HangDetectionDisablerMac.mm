/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HangDetectionDisabler.h"

#if PLATFORM(MAC)

#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RetainPtr.h>

namespace WebKit {

static const auto clientsMayIgnoreEventsKey = CFSTR("ClientMayIgnoreEvents");

static bool clientsMayIgnoreEvents()
{
    CFTypeRef valuePtr;
    if (CGSCopyConnectionProperty(CGSMainConnectionID(), CGSMainConnectionID(), clientsMayIgnoreEventsKey, &valuePtr) != kCGErrorSuccess)
        return false;

    return adoptCF(valuePtr) == kCFBooleanTrue;
}

static void setClientsMayIgnoreEvents(bool clientsMayIgnoreEvents)
{
    auto cgsId = CGSMainConnectionID();
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    // In macOS 10.14 and later, the WebContent process does not have access to the WindowServer.
    // In this case, there will be no valid WindowServer main connection.
    if (!cgsId)
        return;
    // FIXME: <https://webkit.org/b/184484> We should assert here if this is being called from
    // the WebContent process.
#endif
    if (CGSSetConnectionProperty(cgsId, cgsId, clientsMayIgnoreEventsKey, clientsMayIgnoreEvents ? kCFBooleanTrue : kCFBooleanFalse) != kCGErrorSuccess)
        ASSERT_NOT_REACHED();
}

HangDetectionDisabler::HangDetectionDisabler()
    : m_clientsMayIgnoreEvents(clientsMayIgnoreEvents())
{
    setClientsMayIgnoreEvents(true);
}

HangDetectionDisabler::~HangDetectionDisabler()
{
    setClientsMayIgnoreEvents(m_clientsMayIgnoreEvents);
}

}

#endif
