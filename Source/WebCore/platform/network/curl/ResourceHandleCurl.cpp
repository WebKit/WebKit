/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * All rights reserved.
 * Copyright (C) 2017 NAVER Corp.
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
#include "ResourceHandle.h"

#if USE(CURL)

#include "ResourceHandleInternal.h"
#include "SecurityOrigin.h"

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal() = default;

ResourceHandle::~ResourceHandle()
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::platformContinueSynchronousDidReceiveResponse()
{
    ASSERT_NOT_REACHED();
}

bool ResourceHandle::start()
{
    ASSERT_NOT_REACHED();
    return false;
}

void ResourceHandle::cancel()
{
    ASSERT_NOT_REACHED();
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    ASSERT_NOT_REACHED();
    return false;
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, SecurityOrigin*, ResourceError&, ResourceResponse&, Vector<uint8_t>&)
{
    ASSERT_NOT_REACHED();
}

}

#endif
