/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import "CodeSigning.h"

#if PLATFORM(COCOA)

#import <wtf/RetainPtr.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/spi/darwin/CodeSignSPI.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

static String codeSigningIdentifier(SecTaskRef task)
{
    return adoptCF(SecTaskCopySigningIdentifier(task, nullptr)).get();
}

String codeSigningIdentifierForCurrentProcess()
{
    return codeSigningIdentifier(adoptCF(SecTaskCreateFromSelf(kCFAllocatorDefault)).get());
}

String codeSigningIdentifier(xpc_connection_t connection)
{
    auto pair = codeSigningIdentifierAndPlatformBinaryStatus(connection);
    return pair.first;
}

bool currentProcessIsPlatformBinary()
{
    auto task = adoptCF(SecTaskCreateFromSelf(kCFAllocatorDefault));
    return SecTaskGetCodeSignStatus(task.get()) & CS_PLATFORM_BINARY;
}

static std::pair<String, bool> codeSigningIdentifierAndPlatformBinaryStatus(audit_token_t auditToken)
{
    auto task = adoptCF(SecTaskCreateWithAuditToken(kCFAllocatorDefault, auditToken));
    bool isPlatformBinary = SecTaskGetCodeSignStatus(task.get()) & CS_PLATFORM_BINARY;
    auto signingIdentifier = codeSigningIdentifier(task.get());
    return std::make_pair(signingIdentifier, isPlatformBinary);
}

std::pair<String, bool> codeSigningIdentifierAndPlatformBinaryStatus(xpc_connection_t connection)
{
    audit_token_t auditToken;
    xpc_connection_get_audit_token(connection, &auditToken);

    return codeSigningIdentifierAndPlatformBinaryStatus(auditToken);
}

String codeSigningIdentifier(audit_token_t token)
{
    auto pair = codeSigningIdentifierAndPlatformBinaryStatus(token);
    return pair.first;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
