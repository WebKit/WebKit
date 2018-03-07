/*
 * Copyright (C) 2018 Igalia, S.L. All rights reserved.
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

#include "Utilities.h"
#include "WTFStringUtilities.h"

#include <WebCore/DNS.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace TestWebKitAPI {

bool done = false;

TEST(DNSTest, cancelResolveDNS)
{
    RunLoop::initializeMainRunLoop();

    const String hostname = String("www.webkit.org");
    uint64_t identifier = 1;

    DNSCompletionHandler completionHandler = [](WebCore::DNSAddressesOrError&& result) mutable {
        done = true;

        ASSERT_FALSE(result.has_value());
        ASSERT_TRUE(result.error() == WebCore::DNSError::Cancelled);

        return;
    };

    done = false;
    resolveDNS(hostname, identifier, WTFMove(completionHandler));
    stopResolveDNS(identifier);
    Util::run(&done);
}

TEST(DNSTest, cannotResolveDNS)
{
    RunLoop::initializeMainRunLoop();

    const String hostname = String("notavaliddomain.notavaliddomain");
    uint64_t identifier = 1;

    DNSCompletionHandler completionHandler = [](WebCore::DNSAddressesOrError&& result) mutable {
        done = true;

        ASSERT_FALSE(result.has_value());
        ASSERT_TRUE(result.error() == WebCore::DNSError::CannotResolve);

        return;
    };

    done = false;
    resolveDNS(hostname, identifier, WTFMove(completionHandler));
    Util::run(&done);
}

TEST(DNSTest, resolveDNS)
{
    RunLoop::initializeMainRunLoop();

    const String hostname = String("www.webkit.org");
    uint64_t identifier = 1;

    DNSCompletionHandler completionHandler = [](WebCore::DNSAddressesOrError&& result) mutable {
        done = true;

        ASSERT_TRUE(result.has_value());

        return;
    };

    done = false;
    resolveDNS(hostname, identifier, WTFMove(completionHandler));
    Util::run(&done);
}

} // namespace TestWebKitAPI
