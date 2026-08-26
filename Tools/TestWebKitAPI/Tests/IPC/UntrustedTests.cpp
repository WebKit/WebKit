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

#include "Helpers/Test.h"
#include "Untrusted.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/URL.h>

namespace TestWebKitAPI {

// Stands in for a struct declared in a .serialization.in file. The coder below is written by
// hand in the shape generate-serializers.py emits, so that this exercises IPC::Untrusted
// rather than the generator; the generator's own output is covered by the expectations in
// Source/WebKit/Scripts/webkit/tests.
struct UntrustedTestCarrier {
    WebCore::SecurityOriginData origin;
    URL url;
    std::optional<WebCore::SecurityOriginData> optionalOrigin;
};

// A validator is only permitted to derive from IPC::CanValidateUntrusted inside the headers
// listed in Scripts/webkit/untrusted_origins.py, which covers Source/WebKit and not this test.
class RejectingDomainAuthority : public IPC::CanValidateUntrusted<RejectingDomainAuthority> {
public:
    explicit RejectingDomainAuthority(String rejectedHost)
        : m_rejectedHost(WTF::move(rejectedHost))
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::SecurityOriginData& origin) const
    {
        ++visitCount;
        if (origin.host() == m_rejectedHost)
            return IPC::ValidationFailure::Terminate;
        return std::nullopt;
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const URL& url) const
    {
        ++visitCount;
        if (url.host() == m_rejectedHost)
            return IPC::ValidationFailure::Ignore;
        return std::nullopt;
    }

    mutable unsigned visitCount { 0 };

private:
    String m_rejectedHost;
};

// Says nothing about URLs, so it may check an origin but not the carrier that contains one.
class OriginOnlyAuthority : public IPC::CanValidateUntrusted<OriginOnlyAuthority> {
public:
    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::SecurityOriginData&) const
    {
        return std::nullopt;
    }
};

} // namespace TestWebKitAPI

namespace IPC {

template<> struct ArgumentCoder<TestWebKitAPI::UntrustedTestCarrier> {
    static constexpr OptionSet<UntrustedValueKind> untrustedValueKinds { UntrustedValueKind::URL, UntrustedValueKind::SecurityOriginData };

    static void visitUntrustedValues(const TestWebKitAPI::UntrustedTestCarrier& instance, UntrustedValueVisitor& visitor)
    {
        visitor.visitUntrusted(instance.origin);
        visitor.visitUntrusted(instance.url);
        if (instance.optionalOrigin)
            visitor.visitUntrusted((*instance.optionalOrigin));
    }
};

} // namespace IPC

namespace TestWebKitAPI {

static_assert(IPC::CarriesUntrustedValues<UntrustedTestCarrier>);
static_assert(IPC::IsValidationProcedureFor<RejectingDomainAuthority, UntrustedTestCarrier>::value);

// An authority that does not account for every kind the carrier holds may not validate it.
// checkAnyUntrusted static_asserts on that rather than the trait reporting false, so that the
// diagnostic names the missing disposition instead of just failing to find validate().
static_assert(!IPC::checksUntrustedKinds<OriginOnlyAuthority>(IPC::ArgumentCoder<UntrustedTestCarrier>::untrustedValueKinds));
static_assert(IPC::checksUntrustedKinds<RejectingDomainAuthority>(IPC::ArgumentCoder<UntrustedTestCarrier>::untrustedValueKinds));

static UntrustedTestCarrier carrier(ASCIILiteral origin, ASCIILiteral url, std::optional<ASCIILiteral> optionalOrigin = std::nullopt)
{
    return UntrustedTestCarrier {
        WebCore::SecurityOriginData::fromURL(URL { origin }),
        URL { url },
        optionalOrigin ? std::optional { WebCore::SecurityOriginData::fromURL(URL { *optionalOrigin }) } : std::nullopt,
    };
}

TEST(Untrusted, StructWithNoRejectedValueIsValidated)
{
    IPC::Untrusted<UntrustedTestCarrier> untrusted { carrier("https://a.example"_s, "https://a.example/x"_s) };
    RejectingDomainAuthority authority { "evil.example"_s };
    auto validated = WTF::move(untrusted).validate(authority);
    ASSERT_TRUE(validated.hasValue());
    EXPECT_STREQ(validated->origin.host().utf8().data(), "a.example");
    EXPECT_EQ(authority.visitCount, 2u);
}

TEST(Untrusted, RejectedFieldRejectsTheWholeStruct)
{
    IPC::Untrusted<UntrustedTestCarrier> rejectedOrigin { carrier("https://evil.example"_s, "https://a.example/x"_s) };
    auto validated = WTF::move(rejectedOrigin).validate(RejectingDomainAuthority { "evil.example"_s });
    EXPECT_FALSE(validated.hasValue());
    EXPECT_EQ(validated.error(), IPC::ValidationFailure::Terminate);

    IPC::Untrusted<UntrustedTestCarrier> rejectedURL { carrier("https://a.example"_s, "https://evil.example/x"_s) };
    auto urlValidated = WTF::move(rejectedURL).validate(RejectingDomainAuthority { "evil.example"_s });
    EXPECT_FALSE(urlValidated.hasValue());
    EXPECT_EQ(urlValidated.error(), IPC::ValidationFailure::Ignore);

    IPC::Untrusted<UntrustedTestCarrier> rejectedOptional { carrier("https://a.example"_s, "https://a.example/x"_s, "https://evil.example"_s) };
    EXPECT_FALSE(WTF::move(rejectedOptional).validate(RejectingDomainAuthority { "evil.example"_s }).hasValue());
}

// The first failure decides the result; later fields are visited but cannot overwrite it.
TEST(Untrusted, FirstFailureIsReported)
{
    IPC::Untrusted<UntrustedTestCarrier> untrusted { carrier("https://evil.example"_s, "https://evil.example/x"_s) };
    auto validated = WTF::move(untrusted).validate(RejectingDomainAuthority { "evil.example"_s });
    ASSERT_FALSE(validated.hasValue());
    EXPECT_EQ(validated.error(), IPC::ValidationFailure::Terminate);
}

TEST(Untrusted, OptionalAndSetLiftsApplyToSingleValues)
{
    RejectingDomainAuthority authority { "evil.example"_s };
    auto origin = [](ASCIILiteral url) {
        return WebCore::SecurityOriginData::fromURL(URL { url });
    };

    EXPECT_FALSE(authority.checkAnyUntrusted(std::optional<WebCore::SecurityOriginData> { std::nullopt }));
    EXPECT_TRUE(authority.checkAnyUntrusted(std::optional { origin("https://evil.example"_s) }));
    EXPECT_FALSE(authority.checkAnyUntrusted(std::optional { origin("https://a.example"_s) }));

    EXPECT_FALSE(authority.checkAnyUntrusted(HashSet<WebCore::SecurityOriginData> { origin("https://a.example"_s) }));
    EXPECT_TRUE(authority.checkAnyUntrusted(HashSet<WebCore::SecurityOriginData> {
        origin("https://a.example"_s), origin("https://evil.example"_s) }));
}

} // namespace TestWebKitAPI
