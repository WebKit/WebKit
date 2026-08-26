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

#pragma once

#include "UntrustedValueVisitor.h"
#include <concepts>
#include <optional>
#include <wtf/Assertions.h>
#include <wtf/Expected.h>
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Variant.h>

namespace IPC {

template<typename> struct ArgumentCoder;

enum class ValidationFailure : uint8_t {
    // An expected race rather than evidence of an attack; ignore the message.
    Ignore,
    // The sender should not have been able to produce this value.
    Terminate,
};

template<typename T> using Validated = Expected<T, ValidationFailure>;

enum class UnvalidatedReason : uint8_t {
    // Not worked out yet: a bootstrapping state for a sweep that has not been burned down.
    NeedsReview,
    // Another check, here or in the handler this one delegates to, already establishes it.
    ValidatedElsewhere,
    // Names where a request is going or came from, not what the sender may act as.
    RequestTarget,
    // Nothing the privileged process does with it depends on who may speak for it.
    NotSecuritySensitive,
    // Decoded only to be printed by the IPC testing API, which is not enabled in shipping builds.
    IPCTestingAPIIntrospection,
};

// True unless the value could not have been legitimate. A validation that fails with
// ValidationFailure::Ignore lost a race rather than lied, so the message is dropped without
// implicating the sender; see the EXTRACT_WITH_MESSAGE_CHECK macros.
template<typename T> bool valueMayBeLegitimate(const Validated<T>& validated)
{
    return validated.has_value() || validated.error() == ValidationFailure::Ignore;
}

template<typename Validator, typename T> struct IsPreordainedValidator : std::false_type { };

template<typename Validator, typename T>
concept PreordainedValidator = IsPreordainedValidator<std::remove_cvref_t<Validator>, T>::value
    && requires (const std::remove_cvref_t<Validator>& validator, T&& value) {
        { validator.validateUntrusted(WTF::move(value)) } -> std::same_as<Validated<T>>;
    };

// Wraps a value web content sent a privileged process, so it cannot be used until a pre-ordained
// procedure has confirmed the sender was entitled to name it.
template<typename T>
class Untrusted {
public:
    explicit Untrusted(T&& value)
        : m_value(WTF::move(value))
    {
    }

    template<typename Validator> requires PreordainedValidator<Validator, T>
    Validated<T> validate(const Validator& validator) &&
    {
        return validator.validateUntrusted(WTF::move(m_value));
    }

    T unsafeExtractWithoutValidation(UnvalidatedReason) &&
    {
        return WTF::move(m_value);
    }

private:
    friend struct ArgumentCoder<Untrusted<T>>;

    T m_value;
};

template<typename T> struct ArgumentCoder<Untrusted<T>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, const Untrusted<T>& untrusted)
    {
        encoder << untrusted.m_value;
    }

    template<typename Decoder>
    static std::optional<Untrusted<T>> decode(Decoder& decoder)
    {
        auto value = decoder.template decode<T>();
        if (!value)
            return std::nullopt;
        return Untrusted<T> { WTF::move(*value) };
    }
};

// Records that a value needs no check in this context. The reason is not used at runtime; it
// exists so the claim is stated positively and can be audited alongside
// Untrusted::unsafeExtractWithoutValidation.
inline std::optional<ValidationFailure> unvalidated(UnvalidatedReason)
{
    return std::nullopt;
}

template<typename> struct IsStdOptional : std::false_type { };
template<typename T> struct IsStdOptional<std::optional<T>> : std::true_type { };

template<typename> struct IsVariant : std::false_type { };
template<typename... Ts> struct IsVariant<Variant<Ts...>> : std::true_type { };

template<typename Validator, typename T>
constexpr bool checksUntrustedKind(OptionSet<UntrustedValueKind> kinds, UntrustedValueKind kind)
{
    if (!kinds.contains(kind))
        return true;
    return requires (const Validator& validator, const T& value) { validator.checkUntrusted(value); };
}

// True if the validator states a disposition for every kind of untrusted value the struct can
// actually carry. Kinds the struct cannot contain are not required, so an authority is never
// pushed into claiming something about a value it will never be shown.
template<typename Validator>
constexpr bool checksUntrustedKinds(OptionSet<UntrustedValueKind> kinds)
{
    return checksUntrustedKind<Validator, URL>(kinds, UntrustedValueKind::URL)
        && checksUntrustedKind<Validator, WebCore::ClientOrigin>(kinds, UntrustedValueKind::ClientOrigin)
        && checksUntrustedKind<Validator, WebCore::RegistrableDomain>(kinds, UntrustedValueKind::RegistrableDomain)
        && checksUntrustedKind<Validator, WebCore::SecurityOrigin>(kinds, UntrustedValueKind::SecurityOrigin)
        && checksUntrustedKind<Validator, WebCore::SecurityOriginData>(kinds, UntrustedValueKind::SecurityOriginData)
        && checksUntrustedKind<Validator, WebCore::Site>(kinds, UntrustedValueKind::Site);
}

// An authority states its procedure once, as a check on a single value, and this mixin derives
// the forms Untrusted<T> needs: the value-returning validateUntrusted, the lift over optionals
// and sets, and the lift over any serialized struct that carries such values.
//
// The procedure is a check rather than a transform so that nothing has to be copied, which lets
// an authority check a non-copyable WebCore::SecurityOrigin and lets the same procedure be
// applied to a struct's fields in place.
template<typename Derived>
class UntrustedValidation {
public:
    template<typename T>
    Validated<T> validateUntrusted(T&& value) const
    {
        if (auto failure = checkAnyUntrusted(value))
            return std::unexpected { *failure };
        return Validated<T> { WTF::move(value) };
    }

    template<typename T>
    std::optional<ValidationFailure> checkAnyUntrusted(const T& value) const
    {
        const Derived& validator = static_cast<const Derived&>(*this);
        if constexpr (requires { validator.checkUntrusted(value); })
            return validator.checkUntrusted(value);
        else if constexpr (IsStdOptional<T>::value) {
            if (!value)
                return std::nullopt;
            return checkAnyUntrusted(*value);
        } else if constexpr (IsVariant<T>::value) {
            return WTF::switchOn(value, [&](const auto& alternative) {
                return checkAnyUntrusted(alternative);
            });
        } else if constexpr (CarriesUntrustedValues<T>) {
            static_assert(checksUntrustedKinds<Derived>(ArgumentCoder<T>::untrustedValueKinds),
                "This validator does not say what it does with every kind of untrusted value this struct carries. "
                "Add a checkUntrusted overload for the missing one, returning IPC::unvalidated() with a reason if "
                "it needs no check here.");
            StructVisitor visitor { *this };
            ArgumentCoder<T>::visitUntrustedValues(value, visitor);
            return visitor.failure();
        } else {
            for (auto& item : value) {
                if (auto failure = checkAnyUntrusted(item))
                    return failure;
            }
            return std::nullopt;
        }
    }

private:
    class StructVisitor final : public UntrustedValueVisitor {
    public:
        explicit StructVisitor(const UntrustedValidation& validation)
            : m_validation(validation)
        {
        }

        std::optional<ValidationFailure> failure() const { return m_failure; }

        void visitUntrusted(const URL& value) final { check(value); }
        void visitUntrusted(const WebCore::ClientOrigin& value) final { check(value); }
        void visitUntrusted(const WebCore::RegistrableDomain& value) final { check(value); }
        void visitUntrusted(const WebCore::SecurityOrigin& value) final { check(value); }
        void visitUntrusted(const WebCore::SecurityOriginData& value) final { check(value); }
        void visitUntrusted(const WebCore::Site& value) final { check(value); }

    private:
        template<typename T> void check(const T& value)
        {
            // Unreachable for a kind outside untrustedValueKinds, which checkAnyUntrusted has
            // already required a disposition for. Asserting rather than ignoring the value keeps
            // a generator bug from turning into a silently skipped check.
            if constexpr (requires (const Derived& validator) { validator.checkUntrusted(value); }) {
                if (m_failure)
                    return;
                m_failure = m_validation.checkAnyUntrusted(value);
            } else
                RELEASE_ASSERT_NOT_REACHED();
        }

        const UntrustedValidation& m_validation;
        std::optional<ValidationFailure> m_failure;
    };
};

template<typename Validator, typename T>
struct IsPreordainedValidator<Validator, std::optional<T>> : IsPreordainedValidator<Validator, T> { };

// HashSet's last template parameter is a non-type parameter, so it must be spelled out.
template<typename Validator, typename T, typename HashArg, typename TraitsArg, typename TableTraitsArg, WTF::ShouldValidateKey shouldValidateKey>
struct IsPreordainedValidator<Validator, HashSet<T, HashArg, TraitsArg, TableTraitsArg, shouldValidateKey>> : IsPreordainedValidator<Validator, T> { };

template<typename Validator, typename... Ts>
struct IsPreordainedValidator<Validator, Variant<Ts...>>
    : std::bool_constant<(IsPreordainedValidator<Validator, Ts>::value && ...)> { };

// Any authority can validate a struct that carries untrusted values, because the generated
// visitor presents them one at a time and UntrustedValidation applies the authority's own
// procedure to each. What confines this is UntrustedValidation itself: deriving from it is
// only permitted in the headers listed in Scripts/webkit/untrusted_origins.py.
template<typename Validator, typename T> requires CarriesUntrustedValues<T>
struct IsPreordainedValidator<Validator, T> : std::is_base_of<UntrustedValidation<Validator>, Validator> { };

} // namespace IPC

// A translation unit that validates untrusted values defines the following over its own
// MESSAGE_CHECK, whose argument order varies between receivers. They expand to two
// declarations around a check, so they are statements that declare names and cannot be
// brace-less if/else bodies.
//
//     #define EXTRACT_WITH_MESSAGE_CHECK(name, untrusted, ...) \
//         auto name##Validated = WTF::move(untrusted).validate(__VA_ARGS__); \
//         MESSAGE_CHECK(name##Validated); \
//         auto name = WTF::move(*name##Validated)
//
// The validator is the trailing variadic argument because it is brace-initialised and the
// preprocessor splits on the commas inside the braces.
