/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#import <wtf/cocoa/Entitlements.h>

#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/WTFString.h>

namespace WTF {

static bool hasEntitlement(SecTaskRef task, const char* entitlement)
{
    if (!task)
        return false;
    auto string = adoptCF(CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, entitlement, kCFStringEncodingASCII, kCFAllocatorNull));
    return adoptCF(SecTaskCopyValueForEntitlement(task, string.get(), nullptr)) == kCFBooleanTrue;
}

bool hasEntitlement(audit_token_t token, const char* entitlement)
{
    return hasEntitlement(adoptCF(SecTaskCreateWithAuditToken(kCFAllocatorDefault, token)).get(), entitlement);
}

bool hasEntitlement(xpc_connection_t connection, const char *entitlement)
{
    auto value = adoptOSObject(xpc_connection_copy_entitlement_value(connection, entitlement));
    return value && xpc_get_type(value.get()) == XPC_TYPE_BOOL && xpc_bool_get_value(value.get());
}

bool processHasEntitlement(const char* entitlement)
{
    return hasEntitlement(adoptCF(SecTaskCreateFromSelf(kCFAllocatorDefault)).get(), entitlement);
}

bool hasEntitlementValue(audit_token_t token, const char* entitlement, const char* value)
{
    auto secTaskForToken = adoptCF(SecTaskCreateWithAuditToken(kCFAllocatorDefault, token));
    if (!secTaskForToken)
        return { };

    auto string = adoptCF(CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, entitlement, kCFStringEncodingASCII, kCFAllocatorNull));
    String entitlementValue = dynamic_cf_cast<CFStringRef>(adoptCF(SecTaskCopyValueForEntitlement(secTaskForToken.get(), string.get(), nullptr)).get());
    return entitlementValue == value;
}

} // namespace WTF
