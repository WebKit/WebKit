/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#import "AuthenticationChallengeDispositionCocoa.h"

namespace WebKit {

AuthenticationChallengeDisposition toAuthenticationChallengeDisposition(NSURLSessionAuthChallengeDisposition disposition)
{
    switch (disposition) {
    case NSURLSessionAuthChallengeUseCredential:
        return AuthenticationChallengeDisposition::UseCredential;
    case NSURLSessionAuthChallengePerformDefaultHandling:
        return AuthenticationChallengeDisposition::PerformDefaultHandling;
    case NSURLSessionAuthChallengeCancelAuthenticationChallenge:
        return AuthenticationChallengeDisposition::Cancel;
    case NSURLSessionAuthChallengeRejectProtectionSpace:
        return AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue;
    }
    [NSException raise:NSInvalidArgumentException format:@"Invalid NSURLSessionAuthChallengeDisposition (%ld)", (long)disposition];
}

NSURLSessionAuthChallengeDisposition fromAuthenticationChallengeDisposition(AuthenticationChallengeDisposition disposition)
{
    switch (disposition) {
    case AuthenticationChallengeDisposition::UseCredential:
        return NSURLSessionAuthChallengeUseCredential;
    case AuthenticationChallengeDisposition::PerformDefaultHandling:
        return NSURLSessionAuthChallengePerformDefaultHandling;
    case AuthenticationChallengeDisposition::Cancel:
        return NSURLSessionAuthChallengeCancelAuthenticationChallenge;
    case AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue:
        return NSURLSessionAuthChallengeRejectProtectionSpace;
    }
    [NSException raise:NSInvalidArgumentException format:@"Invalid AuthenticationChallengeDisposition (%ld)", (long)disposition];
}

} // namespace WebKit
