/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ChildProcess.h"

#import "WebKitSystemInterface.h"
#import <mach/task.h>

namespace WebKit {

void ChildProcess::setApplicationIsOccluded(bool applicationIsOccluded)
{
    if (this->applicationIsOccluded() == applicationIsOccluded)
        return;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    if (applicationIsOccluded)
        m_processVisibleAssertion.clear();
    else
        m_processVisibleAssertion = WKNSProcessInfoProcessAssertionWithTypes(WKProcessAssertionTypeVisible);
#endif
}

void ChildProcess::platformInitialize()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    setpriority(PRIO_DARWIN_PROCESS, 0, 0);
#endif
    setApplicationIsOccluded(false);
}

}
