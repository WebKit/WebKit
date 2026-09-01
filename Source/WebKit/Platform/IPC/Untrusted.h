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
#include <expected>
#include <optional>
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

template<typename Derived> class CanValidateUntrusted;

// The outcome of a validation procedure: either the value, or why it was rejected.
template<typename T>
class Validated {
public:
    bool hasValue() const { return m_result.has_value(); }
    explicit operator bool() const { return m_result.has_value(); }
    ValidationFailure error() const { return m_result.error(); }

    T& operator*() { return *m_result; }
    const T& operator*() const { return *m_result; }
    T* operator->() { return &*m_result; }
    const T* operator->() const { return &*m_result; }

private:
    // This is the reason this isn't just "using ... = std::expected<...>"
    template<typename> friend class CanValidateUntrusted;

    explicit Validated(T&& value)
        : m_result(WTF::move(value))
    {
    }

    explicit Validated(ValidationFailure failure)
        : m_result(std::unexpected { failure })
    {
    }

    std::expected<T, ValidationFailure> m_result;
};

enum class UnvalidatedReason : uint8_t {
    // Not worked out yet: a bootstrapping state for a sweep that has not been burned down.
    NeedsReview,
    // Another check, here or in the handler this one delegates to, already establishes it.
    // You MUST document where.
    ValidatedElsewhere,
    // Names where a request is going or came from, not what the sender may act as.
    RequestTarget,
    // Nothing the privileged process does with it depends on who may speak for it.
    NotSecuritySensitive,
#if ENABLE(IPC_TESTING_API)
    IPCTestingAPIIntrospection,
#endif
};

// True unless the value could not have been legitimate. A validation that fails with
// ValidationFailure::Ignore lost a race rather than lied, so the message is dropped without
// implicating the sender; see the EXTRACT_WITH_MESSAGE_CHECK macros.
template<typename T> bool valueMayBeLegitimate(const Validated<T>& validated)
{
    return validated.hasValue() || validated.error() == ValidationFailure::Ignore;
}

// Validation procedures need to be explicitly declared such that there is no
// accidental conversion of an IPC::Untrusted<T> to a T, without a considered step.
// Each declaration covers one type only: an authority trusted to vouch for a Site has said
// nothing about a URL.
template<typename Validator, typename T> struct IsValidationProcedureFor : std::false_type { };

// The shape half of ValidationProcedure: the validator hands back a Validated<T> rather than a
// bare T. This cannot tell whether the validator has a check for T, because CanValidateUntrusted
// always declares validateUntrusted.
template<typename Validator, typename T>
concept ProducesValidated = requires(const std::remove_cvref_t<Validator>& validator, T&& value)
{
    { validator.validateUntrusted(WTF::move(value)) } -> std::same_as<Validated<T>>;
};

template<typename Validator, typename T>
concept ValidationProcedure = IsValidationProcedureFor<std::remove_cvref_t<Validator>, T>::value
    && ProducesValidated<Validator, T>;

// Wraps a value a web content process sent to a privileged process, so it cannot be used until
// a designated validation procedure has confirmed the sender was entitled to name it.
template<typename T>
class Untrusted {
public:
    explicit Untrusted(T&& value)
        : m_value(WTF::move(value))
    {
    }

    template<typename Validator> requires ValidationProcedure<Validator, T>
    Validated<T> validate(const Validator& validator) && { return validator.validateUntrusted(WTF::move(m_value)); }

    T unsafeExtractWithoutValidation(UnvalidatedReason) && { return WTF::move(m_value); }

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

// Base class for validation procedures, the only things that can turn an IPC::Untrusted<T> into
// an IPC::Validated<T>.
template<typename Derived>
class CanValidateUntrusted {
public:
    template<typename T>
    Validated<T> validateUntrusted(T&& value) const
    {
        if (auto failure = checkAnyUntrusted(value))
            return Validated<T> { *failure };
        return Validated<T> { WTF::move(value) };
    }

    template<typename T>
    std::optional<ValidationFailure> checkAnyUntrusted(const T& value) const
    {
        const Derived& validator = static_cast<const Derived&>(*this);
        if constexpr (requires { validator.checkUntrusted(value); })
            return validator.checkUntrusted(value);
        else if constexpr (WTF::IsTemplate<T, std::optional>::value) {
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

// A procedure declared for T also covers a container of T, because CanValidateUntrusted applies
// the same check to each element.
template<typename Validator, typename T>
struct IsValidationProcedureFor<Validator, std::optional<T>> : IsValidationProcedureFor<Validator, T> { };

// HashSet's last template parameter is a non-type parameter, so it must be spelled out.
template<typename Validator, typename T, typename HashArg, typename TraitsArg, typename TableTraitsArg, WTF::ShouldValidateKey shouldValidateKey>
struct IsValidationProcedureFor<Validator, HashSet<T, HashArg, TraitsArg, TableTraitsArg, shouldValidateKey>> : IsValidationProcedureFor<Validator, T> { };

} // namespace IPC
