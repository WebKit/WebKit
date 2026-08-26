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

#include <concepts>
#include <optional>
#include <wtf/Expected.h>
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>

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

template<typename> struct IsStdOptional : std::false_type { };
template<typename T> struct IsStdOptional<std::optional<T>> : std::true_type { };

// An authority states its procedure once, as a check on a single value, and this mixin derives
// the forms Untrusted<T> needs: the value-returning validateUntrusted, and the lift over
// optionals and sets.
//
// The procedure is a check rather than a transform so that nothing has to be copied, which lets
// an authority check a non-copyable WebCore::SecurityOrigin.
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
        } else {
            for (auto& item : value) {
                if (auto failure = checkAnyUntrusted(item))
                    return failure;
            }
            return std::nullopt;
        }
    }
};

template<typename Validator, typename T>
struct IsPreordainedValidator<Validator, std::optional<T>> : IsPreordainedValidator<Validator, T> { };

// HashSet's last template parameter is a non-type parameter, so it must be spelled out.
template<typename Validator, typename T, typename HashArg, typename TraitsArg, typename TableTraitsArg, WTF::ShouldValidateKey shouldValidateKey>
struct IsPreordainedValidator<Validator, HashSet<T, HashArg, TraitsArg, TableTraitsArg, shouldValidateKey>> : IsPreordainedValidator<Validator, T> { };

} // namespace IPC
