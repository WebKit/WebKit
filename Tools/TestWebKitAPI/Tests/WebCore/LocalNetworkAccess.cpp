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

#include <WebCore/ClientOrigin.h>
#include <WebCore/IPAddressSpace.h>
#include <WebCore/LocalNetworkAccess.h>
#include <WebCore/PermissionState.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>

namespace TestWebKitAPI {

using namespace WebCore;

using PermissionAnswer = Function<PermissionState(const ClientOrigin&, IPAddressSpace)>;

static PermissionAnswer permissionCheckReturning(PermissionState decision)
{
    return [decision](const ClientOrigin&, IPAddressSpace) {
        return decision;
    };
}

// Mirrors NetworkResourceLoader: the permission is only consulted when the check says one is required.
static std::optional<ResourceError> checkSynchronouslyWithOriginPair(const ResourceRequest& request, IPAddressSpace connectionAddressSpace, IPAddressSpace clientAddressSpace, bool clientIsSecureContext, const ClientOrigin& clientOrigin, const PermissionAnswer& permissionCheck, const URL& currentURL = { }, bool localNetworkAllowedByPermissionsPolicy = true, bool loopbackNetworkAllowedByPermissionsPolicy = true)
{
    auto requirement = checkLocalNetworkAccess(request, currentURL.isNull() ? request.url() : currentURL, connectionAddressSpace, clientAddressSpace, clientIsSecureContext, clientOrigin, localNetworkAllowedByPermissionsPolicy, loopbackNetworkAllowedByPermissionsPolicy);
    if (!requirement)
        return requirement.error();
    if (*requirement == LocalNetworkAccessRequirement::None)
        return std::nullopt;
    return localNetworkAccessPermissionError(request.url(), permissionCheck(clientOrigin, connectionAddressSpace));
}

static std::optional<ResourceError> checkSynchronously(const ResourceRequest& request, IPAddressSpace connectionAddressSpace, IPAddressSpace clientAddressSpace, bool clientIsSecureContext, const SecurityOriginData& clientOrigin, const PermissionAnswer& permissionCheck, const URL& currentURL = { }, bool localNetworkAllowedByPermissionsPolicy = true, bool loopbackNetworkAllowedByPermissionsPolicy = true)
{
    return checkSynchronouslyWithOriginPair(request, connectionAddressSpace, clientAddressSpace, clientIsSecureContext, ClientOrigin { clientOrigin, clientOrigin }, permissionCheck, currentURL, localNetworkAllowedByPermissionsPolicy, loopbackNetworkAllowedByPermissionsPolicy);
}

// A client with no document -- a service worker -- is not refused outright. It cannot prompt, because
// there is no gesture behind its requests, but a decision the user already recorded for its origin
// still applies. Matches what Chromium does for workers, and what
// imported/w3c/.../service-worker.tentative.https asserts.
TEST(LocalNetworkAccess, AClientWithNoDocumentStillUsesARecordedGrant)
{
    ResourceRequest request { URL { "http://192.168.1.1/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    bool consulted = false;
    PermissionAnswer recordingCheck = [&consulted](const ClientOrigin&, IPAddressSpace) {
        consulted = true;
        return PermissionState::Granted;
    };

    auto error = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Public, true, clientOrigin, recordingCheck);
    EXPECT_FALSE(error.has_value());
    EXPECT_TRUE(consulted);
}

TEST(LocalNetworkAccess, PermissionsPolicyDenialBlocksWithoutConsultingPermission)
{
    ResourceRequest request { URL { "http://192.168.1.1/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    bool consulted = false;
    PermissionAnswer recordingCheck = [&consulted](const ClientOrigin&, IPAddressSpace) {
        consulted = true;
        return PermissionState::Granted;
    };

    auto error = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Public, true, clientOrigin, recordingCheck, { }, false, true);
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
    EXPECT_FALSE(consulted);
}

TEST(LocalNetworkAccess, LoopbackPermissionsPolicyIsSeparateFromLocal)
{
    ResourceRequest request { URL { "http://127.0.0.1/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    // Loopback is allowed by policy while local is not, so a loopback target still reaches the
    // permission check.
    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted), { }, false, true);
    EXPECT_FALSE(error.has_value());

    // And the reverse: denying loopback by policy blocks it even when local is allowed.
    auto blocked = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted), { }, true, false);
    ASSERT_TRUE(blocked.has_value());
    EXPECT_TRUE(blocked->isAccessControl());
}

TEST(LocalNetworkAccess, RedirectIntoLocalNetworkIsNotExemptedByTheOriginalURL)
{
    ResourceRequest request { URL { "https://example.com/redirect"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied), URL { "http://192.168.1.1/admin"_s });
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, RedirectStayingSameOriginRemainsExempt)
{
    ResourceRequest request { URL { "https://example.com/redirect"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied), URL { "https://example.com/target"_s });
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, SameOriginTrustworthyExemption)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, SameOriginButNotTrustworthyIsNotExempt)
{
    ResourceRequest request { URL { "http://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "http://example.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, CrossOriginIsNotExempt)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, MatchingTargetAddressSpaceStillRequiresPermission)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    request.setTargetAddressSpace(IPAddressSpace::Loopback);
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, MatchingTargetAddressSpaceAllowedWithGrant)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    request.setTargetAddressSpace(IPAddressSpace::Loopback);
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, LoopbackToLoopbackIsAllowedWithoutExplicitTargetAddressSpace)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Loopback, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

// Loopback is less public than local, so this needs permission. The spec requires it
// ("lhs is loopback and rhs is either local or public"), but Chromium does not enforce the permission
// for cross-origin local requests, so the chrome-normative WPT suite asserts the opposite and
// fetch/local-network-access/websocket.tentative.https.html fails this case on purpose.
// publicnessRank() ranks Unknown alongside Loopback, so an undetermined *client* space would make
// every target not-less-public and skip the check. A document whose own space could not be determined
// -- e.g. served from a cache entry stored without one -- must still be gated.
TEST(LocalNetworkAccess, UndeterminedClientSpaceStillRequiresPermission)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Unknown, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_TRUE(error.has_value());
}

TEST(LocalNetworkAccess, UndeterminedClientSpaceCanStillBeGranted)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Unknown, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, LocalToLoopbackRequiresPermission)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Local, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_TRUE(error.has_value());
}

TEST(LocalNetworkAccess, LocalToLoopbackAllowedWithGrant)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Local, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, LocalToLocalIsAllowed)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Local, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, SecureContextGrantedByPermissionStub)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, SecureContextPromptIsTreatedAsDenied)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Prompt));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

TEST(LocalNetworkAccess, UndeterminedConnectionSpaceIsBlockedByDefaultPermission)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

// The algorithm defers to whatever the permission callback returns, including for an undetermined
// space. That is a statement about this function only: the NetworkProcess-side permission store refuses
// an undetermined space unconditionally and never asks, so this outcome is not reachable in production.
// Kept because the layering is the point -- the policy lives in the caller, not here.
TEST(LocalNetworkAccess, UndeterminedConnectionSpaceCanStillBeGranted)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    EXPECT_FALSE(error.has_value());
}

TEST(LocalNetworkAccess, UndeterminedConnectionSpaceStillHonorsSameOriginTrustworthyExemption)
{
    ResourceRequest request { URL { "https://example.com/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    EXPECT_FALSE(error.has_value());
}

// Exhaustive over the inputs, because the ordering asserted separately below is only meaningful if
// no other combination is doing something unexpected.
TEST(LocalNetworkAccess, PermissionRequestOutcomeCoversEveryInput)
{
    using Outcome = LocalNetworkAccessPermissionRequestOutcome;
    struct Expectation {
        IPAddressSpace addressSpace;
        bool hasRecordedDecision;
        Outcome expected;
    };

    static constexpr Expectation expectations[] = {
        // An undetermined space refuses regardless of what else is true.
        { IPAddressSpace::Unknown, false, Outcome::RefuseAsUndetermined },
        { IPAddressSpace::Unknown, true, Outcome::RefuseAsUndetermined },

        { IPAddressSpace::Public, false, Outcome::Prompt },
        { IPAddressSpace::Public, true, Outcome::UseRecordedDecision },

        { IPAddressSpace::Local, false, Outcome::Prompt },
        { IPAddressSpace::Local, true, Outcome::UseRecordedDecision },

        { IPAddressSpace::Loopback, false, Outcome::Prompt },
        { IPAddressSpace::Loopback, true, Outcome::UseRecordedDecision },
    };

    for (auto& expectation : expectations) {
        auto outcome = localNetworkAccessPermissionRequestOutcome(expectation.addressSpace, expectation.hasRecordedDecision);
        EXPECT_EQ(expectation.expected, outcome)
            << "addressSpace=" << static_cast<unsigned>(expectation.addressSpace)
            << " recorded=" << expectation.hasRecordedDecision;
    }
}

// The undetermined check has to come first, or a grant recorded for a space the user could name
// would answer for one they could not.
TEST(LocalNetworkAccess, UndeterminedSpaceOutranksARecordedDecision)
{
    EXPECT_EQ(LocalNetworkAccessPermissionRequestOutcome::RefuseAsUndetermined,
        localNetworkAccessPermissionRequestOutcome(IPAddressSpace::Unknown, true));
}

// A request that was never asked about must not be reported as a denial the user made, since that is
// what the console message and the developer's next step hinge on.
TEST(LocalNetworkAccess, NotAskedIsReportedDifferentlyFromDenied)
{
    ResourceRequest request { URL { "http://192.168.1.1/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto notAsked = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Prompt));
    ASSERT_TRUE(notAsked.has_value());
    EXPECT_TRUE(notAsked->isAccessControl());
    EXPECT_TRUE(notAsked->localizedDescription().contains("no document to ask in"_s));

    auto denied = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(denied.has_value());
    EXPECT_TRUE(denied->isAccessControl());
    EXPECT_TRUE(denied->localizedDescription().contains("was denied"_s));
    EXPECT_FALSE(denied->localizedDescription().contains("no document to ask in"_s));
}

// Each refusal reaching the console names its own cause, so a developer is not told the frame lacks
// the feature when the real problem is that there is no document.
TEST(LocalNetworkAccess, EachRefusalNamesItsOwnCause)
{
    ResourceRequest request { URL { "http://192.168.1.1/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto policyRefused = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted), { }, false, true);
    ASSERT_TRUE(policyRefused.has_value());
    EXPECT_TRUE(policyRefused->localizedDescription().contains("\"local-network\""_s));

    auto insecure = checkSynchronously(request, IPAddressSpace::Local, IPAddressSpace::Public, false, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    ASSERT_TRUE(insecure.has_value());
    EXPECT_TRUE(insecure->localizedDescription().contains("not a secure context"_s));

    // An undetermined connection space skips the publicness comparison entirely, so the refusal must
    // not claim a less-public relation that was never established.
    auto undetermined = checkSynchronously(request, IPAddressSpace::Unknown, IPAddressSpace::Public, false, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    ASSERT_TRUE(undetermined.has_value());
    EXPECT_TRUE(undetermined->localizedDescription().contains("not a secure context"_s));
}

// A request that passes every policy check is not decided by the check itself: it reports that the
// embedder has to be asked, and the answer is turned into the refusal separately.
TEST(LocalNetworkAccess, AGatedRequestDefersToThePermissionAnswer)
{
    ResourceRequest request { URL { "http://192.168.1.1/"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto requirement = checkLocalNetworkAccess(request, request.url(), IPAddressSpace::Local, IPAddressSpace::Public, true, ClientOrigin { clientOrigin, clientOrigin }, true, true);
    ASSERT_TRUE(requirement.has_value());
    EXPECT_EQ(LocalNetworkAccessRequirement::Permission, *requirement);

    EXPECT_FALSE(localNetworkAccessPermissionError(request.url(), PermissionState::Granted).has_value());
    auto denied = localNetworkAccessPermissionError(request.url(), PermissionState::Denied);
    ASSERT_TRUE(denied.has_value());
    EXPECT_TRUE(denied->isAccessControl());
    EXPECT_TRUE(denied->localizedDescription().contains("was denied"_s));
    EXPECT_EQ(request.url().string(), denied->failingURL().string());
}

// The exemption must compare the URL against the *requesting* origin. Every other test passes the same
// origin for both halves of the pair, so swapping the two members here would go unnoticed -- and
// topOrigin is the member the grant store keys on, which makes it the plausible slip.
TEST(LocalNetworkAccess, SameOriginExemptionComparesTheRequestingOriginNotTheTopLevelOne)
{
    ResourceRequest request { URL { "http://127.0.0.1/admin"_s } };
    ClientOrigin embeddedInLoopbackPage {
        SecurityOriginData::fromURL(URL { "http://127.0.0.1/"_s }),
        SecurityOriginData::fromURL(URL { "https://example.com/"_s })
    };

    // The connection URL is same-origin with the TOP origin but not the requesting one, so the
    // exemption must not fire and the permission must decide.
    auto error = checkSynchronouslyWithOriginPair(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, embeddedInLoopbackPage, permissionCheckReturning(PermissionState::Denied));
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

// The companion: a redirect into a trustworthy loopback URL. Unlike a 192.168 target, this one is
// potentially trustworthy, so the refusal rests entirely on the origin comparison rather than on the
// trustworthiness conjunct short-circuiting.
TEST(LocalNetworkAccess, RedirectIntoTrustworthyLoopbackIsNotExemptedByOrigin)
{
    ResourceRequest request { URL { "https://example.com/redirect"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    auto error = checkSynchronously(request, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Denied), URL { "http://127.0.0.1/admin"_s });
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->isAccessControl());
}

// A declared targetAddressSpace that does not match the resolved connection is refused before the
// publicness comparison, so a page cannot reach a local address on a loopback grant. Asserted in a
// secure context and on the message, because a non-secure client refuses for its own reason and would
// mask the loss of this check entirely.
TEST(LocalNetworkAccess, DeclaredTargetAddressSpaceMustMatchTheConnection)
{
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://example.com/"_s });

    ResourceRequest declaredLoopback { URL { "https://other.com/resource"_s } };
    declaredLoopback.setTargetAddressSpace(IPAddressSpace::Loopback);
    auto reachedLocal = checkSynchronously(declaredLoopback, IPAddressSpace::Local, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    ASSERT_TRUE(reachedLocal.has_value());
    EXPECT_TRUE(reachedLocal->localizedDescription().contains("does not match"_s));

    ResourceRequest declaredLocal { URL { "https://other.com/resource"_s } };
    declaredLocal.setTargetAddressSpace(IPAddressSpace::Local);
    auto reachedLoopback = checkSynchronously(declaredLocal, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted));
    ASSERT_TRUE(reachedLoopback.has_value());
    EXPECT_TRUE(reachedLoopback->localizedDescription().contains("does not match"_s));

    // Declaring Public is the default and asserts nothing, so any connection is allowed to proceed to
    // the permission check.
    ResourceRequest declaredPublic { URL { "https://other.com/resource"_s } };
    declaredPublic.setTargetAddressSpace(IPAddressSpace::Public);
    EXPECT_FALSE(checkSynchronously(declaredPublic, IPAddressSpace::Loopback, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted)).has_value());
}

// Neither allow path may consult the permission. Reaching it for an ordinary same-origin or
// public-to-public request would mean a prompt during normal browsing, which is the prompt fatigue the
// whole design is trying to avoid.
TEST(LocalNetworkAccess, TheAllowPathsDoNotConsultThePermission)
{
    bool consulted = false;
    PermissionAnswer recordingCheck = [&consulted](const ClientOrigin&, IPAddressSpace) {
        consulted = true;
        return PermissionState::Granted;
    };

    // Same-origin and trustworthy.
    ResourceRequest sameOrigin { URL { "https://example.com/resource"_s } };
    auto sameOriginClient = SecurityOriginData::fromURL(URL { "https://example.com/"_s });
    EXPECT_FALSE(checkSynchronously(sameOrigin, IPAddressSpace::Loopback, IPAddressSpace::Public, true, sameOriginClient, recordingCheck).has_value());
    EXPECT_FALSE(consulted);

    // Not less public than the client.
    ResourceRequest publicToPublic { URL { "https://other.com/resource"_s } };
    EXPECT_FALSE(checkSynchronously(publicToPublic, IPAddressSpace::Public, IPAddressSpace::Public, true, sameOriginClient, recordingCheck).has_value());
    EXPECT_FALSE(consulted);
}

// An undetermined connection space consults the local-network feature, not the loopback one. A frame
// allowed only loopback-network is therefore refused, which is the fail-closed choice but is worth
// pinning because the refusal message names both features and cannot tell you which was consulted.
TEST(LocalNetworkAccess, AnUndeterminedConnectionConsultsTheLocalNetworkFeature)
{
    ResourceRequest request { URL { "https://example.com/resource"_s } };
    auto clientOrigin = SecurityOriginData::fromURL(URL { "https://other.com/"_s });

    auto loopbackOnly = checkSynchronously(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted), { }, false, true);
    ASSERT_TRUE(loopbackOnly.has_value());
    EXPECT_TRUE(loopbackOnly->localizedDescription().contains("not allowed to use"_s));

    auto localAllowed = checkSynchronously(request, IPAddressSpace::Unknown, IPAddressSpace::Public, true, clientOrigin, permissionCheckReturning(PermissionState::Granted), { }, true, false);
    EXPECT_FALSE(localAllowed.has_value());
}

} // namespace TestWebKitAPI
