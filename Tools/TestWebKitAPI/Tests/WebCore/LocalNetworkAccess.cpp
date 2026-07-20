/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/IPAddressSpace.h>
#include <WebCore/LocalNetworkAccess.h>
#include <WebCore/PermissionState.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>

namespace TestWebKitAPI {

using namespace WebCore;

static LocalNetworkAccessPermissionCheckFunction permissionCheckReturning(PermissionState decision)
{
    return [decision](const SecurityOriginData&, IPAddressSpace) {
        return decision;
    };
}

TEST(LocalNetworkAccess, SameOriginTrustworthyExemption)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, SameOriginButNotTrustworthyIsNotExempt)
{
    ResourceRequest request { URL { "http://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "http://example.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, CrossOriginIsNotExempt)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, MatchingTargetAddressSpaceStillRequiresPermission)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    request.setTargetAddressSpace(IPAddressSpace::Loopback);
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, MatchingTargetAddressSpaceAllowedWithGrant)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    request.setTargetAddressSpace(IPAddressSpace::Loopback);
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, MismatchedTargetAddressSpaceIsBlocked)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    request.setTargetAddressSpace(IPAddressSpace::Local);
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, false, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, MorePublicConnectionIsAllowed)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Public, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, LoopbackToLoopbackIsAllowedWithoutExplicitTargetAddressSpace)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Loopback, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, NonSecureContextIsBlockedWithoutConsultingPermissionStub)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, false, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, SecureContextGrantedByPermissionStub)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, SecureContextDeniedByPermissionStub)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, SecureContextPromptIsTreatedAsDenied)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Prompt));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, UndeterminedConnectionSpaceInNonSecureContextIsBlocked)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Unknown, IPAddressSpace::Public, false, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, UndeterminedConnectionSpaceIsBlockedByDefaultPermission)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, UndeterminedConnectionSpaceCanStillBeGranted)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, UndeterminedConnectionSpaceStillHonorsSameOriginTrustworthyExemption)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto error = performLocalNetworkAccessCheck(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

} // namespace TestWebKitAPI
