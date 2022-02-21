/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/ThreadSafeRefCounted.h>

@class NSError;

namespace WebPushD {

enum class PushAppBundleExists : bool {
    No,
    Yes,
};

enum class PushAppBundleCreationResult : bool {
    Failure,
    Success,
};

class PushAppBundle;

class PushAppBundleClient {
public:
    virtual ~PushAppBundleClient();

    virtual void didCheckForExistingBundle(PushAppBundle&, PushAppBundleExists) = 0;
    virtual void didDeleteExistingBundleWithError(PushAppBundle&, NSError *) = 0;
    virtual void didCreateAppBundle(PushAppBundle&, PushAppBundleCreationResult) = 0;
};

class PushAppBundle : public ThreadSafeRefCounted<PushAppBundle> {
public:
    virtual ~PushAppBundle();
    void detachFromClient();

    virtual void checkForExistingBundle() = 0;
    virtual void deleteExistingBundle() = 0;
    virtual void createBundle() = 0;
    virtual void stop() = 0;

protected:
    PushAppBundle(PushAppBundleClient&);

    PushAppBundleClient* m_client;
};


} // namespace WebPushD
