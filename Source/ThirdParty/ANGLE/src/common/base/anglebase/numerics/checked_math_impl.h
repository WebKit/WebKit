// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_CHECKED_MATH_IMPL_H_
#define BASE_NUMERICS_CHECKED_MATH_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <type_traits>

#include "anglebase/numerics/safe_conversions.h"
#include "anglebase/numerics/safe_math_shared_impl.h"

namespace angle
{
namespace base
{
namespace internal
{

template <typename T>
constexpr bool CheckedAddImpl(T x, T y, T *result)
{
    static_assert(std::is_integral<T>::value, "Type must be integral");
    // Since the value of x+y is undefined if we have a signed type, we compute
    // it using the unsigned type of the same size.
    using UnsignedDst         = typename std::make_unsigned<T>::type;
    using SignedDst           = typename std::make_signed<T>::type;
    const UnsignedDst ux      = static_cast<UnsignedDst>(x);
    const UnsignedDst uy      = static_cast<UnsignedDst>(y);
    const UnsignedDst uresult = static_cast<UnsignedDst>(ux + uy);
    // Addition is valid if the sign of (x + y) is equal to either that of x or
    // that of y.
    if (std::is_signed<T>::value ? static_cast<SignedDst>((uresult ^ ux) & (uresult ^ uy)) < 0
                                 : uresult < uy)  // Unsigned is either valid or underflow.
        return false;
    *result = static_cast<T>(uresult);
    return true;
}

template <typename T, typename U, class Enable = void>
struct CheckedAddOp
{};

template <typename T, typename U>
struct CheckedAddOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type = typename MaxExponentPromotion<T, U>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        if constexpr (CheckedAddFastOp<T, U>::is_supported)
            return CheckedAddFastOp<T, U>::Do(x, y, result);

        // Double the underlying type up to a full machine word.
        using FastPromotion = typename FastIntegerArithmeticPromotion<T, U>::type;
        using Promotion =
            typename std::conditional<(IntegerBitsPlusSign<FastPromotion>::value >
                                       IntegerBitsPlusSign<intptr_t>::value),
                                      typename BigEnoughPromotion<T, U>::type, FastPromotion>::type;
        // Fail if either operand is out of range for the promoted type.
        // TODO(jschuh): This could be made to work for a broader range of values.
        if (BASE_NUMERICS_UNLIKELY(!IsValueInRangeForNumericType<Promotion>(x) ||
                                   !IsValueInRangeForNumericType<Promotion>(y)))
        {
            return false;
        }

        Promotion presult = {};
        bool is_valid     = true;
        if constexpr (IsIntegerArithmeticSafe<Promotion, T, U>::value)
        {
            presult = static_cast<Promotion>(x) + static_cast<Promotion>(y);
        }
        else
        {
            is_valid =
                CheckedAddImpl(static_cast<Promotion>(x), static_cast<Promotion>(y), &presult);
        }
        if (!is_valid || !IsValueInRangeForNumericType<V>(presult))
            return false;
        *result = static_cast<V>(presult);
        return true;
    }
};

template <typename T>
constexpr bool CheckedSubImpl(T x, T y, T *result)
{
    static_assert(std::is_integral<T>::value, "Type must be integral");
    // Since the value of x+y is undefined if we have a signed type, we compute
    // it using the unsigned type of the same size.
    using UnsignedDst         = typename std::make_unsigned<T>::type;
    using SignedDst           = typename std::make_signed<T>::type;
    const UnsignedDst ux      = static_cast<UnsignedDst>(x);
    const UnsignedDst uy      = static_cast<UnsignedDst>(y);
    const UnsignedDst uresult = static_cast<UnsignedDst>(ux - uy);
    // Subtraction is valid if either x and y have same sign, or (x-y) and x have
    // the same sign.
    if (std::is_signed<T>::value ? static_cast<SignedDst>((uresult ^ ux) & (ux ^ uy)) < 0 : x < y)
        return false;
    *result = static_cast<T>(uresult);
    return true;
}

template <typename T, typename U, class Enable = void>
struct CheckedSubOp
{};

template <typename T, typename U>
struct CheckedSubOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type = typename MaxExponentPromotion<T, U>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        if constexpr (CheckedSubFastOp<T, U>::is_supported)
            return CheckedSubFastOp<T, U>::Do(x, y, result);

        // Double the underlying type up to a full machine word.
        using FastPromotion = typename FastIntegerArithmeticPromotion<T, U>::type;
        using Promotion =
            typename std::conditional<(IntegerBitsPlusSign<FastPromotion>::value >
                                       IntegerBitsPlusSign<intptr_t>::value),
                                      typename BigEnoughPromotion<T, U>::type, FastPromotion>::type;
        // Fail if either operand is out of range for the promoted type.
        // TODO(jschuh): This could be made to work for a broader range of values.
        if (BASE_NUMERICS_UNLIKELY(!IsValueInRangeForNumericType<Promotion>(x) ||
                                   !IsValueInRangeForNumericType<Promotion>(y)))
        {
            return false;
        }

        Promotion presult = {};
        bool is_valid     = true;
        if constexpr (IsIntegerArithmeticSafe<Promotion, T, U>::value)
        {
            presult = static_cast<Promotion>(x) - static_cast<Promotion>(y);
        }
        else
        {
            is_valid =
                CheckedSubImpl(static_cast<Promotion>(x), static_cast<Promotion>(y), &presult);
        }
        if (!is_valid || !IsValueInRangeForNumericType<V>(presult))
            return false;
        *result = static_cast<V>(presult);
        return true;
    }
};

template <typename T>
constexpr bool CheckedMulImpl(T x, T y, T *result)
{
    static_assert(std::is_integral<T>::value, "Type must be integral");
    // Since the value of x*y is potentially undefined if we have a signed type,
    // we compute it using the unsigned type of the same size.
    using UnsignedDst         = typename std::make_unsigned<T>::type;
    using SignedDst           = typename std::make_signed<T>::type;
    const UnsignedDst ux      = SafeUnsignedAbs(x);
    const UnsignedDst uy      = SafeUnsignedAbs(y);
    const UnsignedDst uresult = static_cast<UnsignedDst>(ux * uy);
    const bool is_negative    = std::is_signed<T>::value && static_cast<SignedDst>(x ^ y) < 0;
    // We have a fast out for unsigned identity or zero on the second operand.
    // After that it's an unsigned overflow check on the absolute value, with
    // a +1 bound for a negative result.
    if (uy > UnsignedDst(!std::is_signed<T>::value || is_negative) &&
        ux > (std::numeric_limits<T>::max() + UnsignedDst(is_negative)) / uy)
        return false;
    *result = is_negative ? 0 - uresult : uresult;
    return true;
}

template <typename T, typename U, class Enable = void>
struct CheckedMulOp
{};

template <typename T, typename U>
struct CheckedMulOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type = typename MaxExponentPromotion<T, U>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        if constexpr (CheckedMulFastOp<T, U>::is_supported)
            return CheckedMulFastOp<T, U>::Do(x, y, result);

        using Promotion = typename FastIntegerArithmeticPromotion<T, U>::type;
        // Verify the destination type can hold the result (always true for 0).
        if (BASE_NUMERICS_UNLIKELY((!IsValueInRangeForNumericType<Promotion>(x) ||
                                    !IsValueInRangeForNumericType<Promotion>(y)) &&
                                   x && y))
        {
            return false;
        }

        Promotion presult = {};
        bool is_valid     = true;
        if constexpr (CheckedMulFastOp<Promotion, Promotion>::is_supported)
        {
            // The fast op may be available with the promoted type.
            is_valid = CheckedMulFastOp<Promotion, Promotion>::Do(x, y, &presult);
        }
        else if (IsIntegerArithmeticSafe<Promotion, T, U>::value)
        {
            presult = static_cast<Promotion>(x) * static_cast<Promotion>(y);
        }
        else
        {
            is_valid =
                CheckedMulImpl(static_cast<Promotion>(x), static_cast<Promotion>(y), &presult);
        }
        if (!is_valid || !IsValueInRangeForNumericType<V>(presult))
            return false;
        *result = static_cast<V>(presult);
        return true;
    }
};

// Division just requires a check for a zero denominator or an invalid negation
// on signed min/-1.
template <typename T, typename U, class Enable = void>
struct CheckedDivOp
{};

template <typename T, typename U>
struct CheckedDivOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type = typename MaxExponentPromotion<T, U>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        if (BASE_NUMERICS_UNLIKELY(!y))
            return false;

        // The overflow check can be compiled away if we don't have the exact
        // combination of types needed to trigger this case.
        using Promotion = typename BigEnoughPromotion<T, U>::type;
        if (BASE_NUMERICS_UNLIKELY(
                (std::is_signed<T>::value && std::is_signed<U>::value &&
                 IsTypeInRangeForNumericType<T, Promotion>::value &&
                 static_cast<Promotion>(x) == std::numeric_limits<Promotion>::lowest() &&
                 y == static_cast<U>(-1))))
        {
            return false;
        }

        // This branch always compiles away if the above branch wasn't removed.
        if (BASE_NUMERICS_UNLIKELY((!IsValueInRangeForNumericType<Promotion>(x) ||
                                    !IsValueInRangeForNumericType<Promotion>(y)) &&
                                   x))
        {
            return false;
        }

        const Promotion presult = Promotion(x) / Promotion(y);
        if (!IsValueInRangeForNumericType<V>(presult))
            return false;
        *result = static_cast<V>(presult);
        return true;
    }
};

template <typename T, typename U, class Enable = void>
struct CheckedModOp
{};

template <typename T, typename U>
struct CheckedModOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type = typename MaxExponentPromotion<T, U>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        if (BASE_NUMERICS_UNLIKELY(!y))
            return false;

        using Promotion = typename BigEnoughPromotion<T, U>::type;
        if (BASE_NUMERICS_UNLIKELY(
                (std::is_signed<T>::value && std::is_signed<U>::value &&
                 IsTypeInRangeForNumericType<T, Promotion>::value &&
                 static_cast<Promotion>(x) == std::numeric_limits<Promotion>::lowest() &&
                 y == static_cast<U>(-1))))
        {
            *result = 0;
            return true;
        }

        const Promotion presult = static_cast<Promotion>(x) % static_cast<Promotion>(y);
        if (!IsValueInRangeForNumericType<V>(presult))
            return false;
        *result = static_cast<Promotion>(presult);
        return true;
    }
};

template <typename T, typename U, class Enable = void>
struct CheckedLshOp
{};

// Left shift. Shifts less than 0 or greater than or equal to the number
// of bits in the promoted type are undefined. Shifts of negative values
// are undefined. Otherwise it is defined when the result fits.
template <typename T, typename U>
struct CheckedLshOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type = T;
    template <typename V>
    static constexpr bool Do(T x, U shift, V *result)
    {
        // Disallow negative numbers and verify the shift is in bounds.
        if (BASE_NUMERICS_LIKELY(!IsValueNegative(x) &&
                                 as_unsigned(shift) < as_unsigned(std::numeric_limits<T>::digits)))
        {
            // Shift as unsigned to avoid undefined behavior.
            *result = static_cast<V>(as_unsigned(x) << shift);
            // If the shift can be reversed, we know it was valid.
            return *result >> shift == x;
        }

        // Handle the legal corner-case of a full-width signed shift of zero.
        if (!std::is_signed<T>::value || x ||
            as_unsigned(shift) != as_unsigned(std::numeric_limits<T>::digits))
            return false;
        *result = 0;
        return true;
    }
};

template <typename T, typename U, class Enable = void>
struct CheckedRshOp
{};

// Right shift. Shifts less than 0 or greater than or equal to the number
// of bits in the promoted type are undefined. Otherwise, it is always defined,
// but a right shift of a negative value is implementation-dependent.
template <typename T, typename U>
struct CheckedRshOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type = T;
    template <typename V>
    static bool Do(T x, U shift, V *result)
    {
        // Use sign conversion to push negative values out of range.
        if (BASE_NUMERICS_UNLIKELY(as_unsigned(shift) >= IntegerBitsPlusSign<T>::value))
        {
            return false;
        }

        const T tmp = x >> shift;
        if (!IsValueInRangeForNumericType<V>(tmp))
            return false;
        *result = static_cast<V>(tmp);
        return true;
    }
};

template <typename T, typename U, class Enable = void>
struct CheckedAndOp
{};

// For simplicity we support only unsigned integer results.
template <typename T, typename U>
struct CheckedAndOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type =
        typename std::make_unsigned<typename MaxExponentPromotion<T, U>::type>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        const result_type tmp = static_cast<result_type>(x) & static_cast<result_type>(y);
        if (!IsValueInRangeForNumericType<V>(tmp))
            return false;
        *result = static_cast<V>(tmp);
        return true;
    }
};

template <typename T, typename U, class Enable = void>
struct CheckedOrOp
{};

// For simplicity we support only unsigned integers.
template <typename T, typename U>
struct CheckedOrOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type =
        typename std::make_unsigned<typename MaxExponentPromotion<T, U>::type>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        const result_type tmp = static_cast<result_type>(x) | static_cast<result_type>(y);
        if (!IsValueInRangeForNumericType<V>(tmp))
            return false;
        *result = static_cast<V>(tmp);
        return true;
    }
};

template <typename T, typename U, class Enable = void>
struct CheckedXorOp
{};

// For simplicity we support only unsigned integers.
template <typename T, typename U>
struct CheckedXorOp<
    T,
    U,
    typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type>
{
    using result_type =
        typename std::make_unsigned<typename MaxExponentPromotion<T, U>::type>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        const result_type tmp = static_cast<result_type>(x) ^ static_cast<result_type>(y);
        if (!IsValueInRangeForNumericType<V>(tmp))
            return false;
        *result = static_cast<V>(tmp);
        return true;
    }
};

// Max doesn't really need to be implemented this way because it can't fail,
// but it makes the code much cleaner to use the MathOp wrappers.
template <typename T, typename U, class Enable = void>
struct CheckedMaxOp
{};

template <typename T, typename U>
struct CheckedMaxOp<
    T,
    U,
    typename std::enable_if<std::is_arithmetic<T>::value && std::is_arithmetic<U>::value>::type>
{
    using result_type = typename MaxExponentPromotion<T, U>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        const result_type tmp =
            IsGreater<T, U>::Test(x, y) ? static_cast<result_type>(x) : static_cast<result_type>(y);
        if (!IsValueInRangeForNumericType<V>(tmp))
            return false;
        *result = static_cast<V>(tmp);
        return true;
    }
};

// Min doesn't really need to be implemented this way because it can't fail,
// but it makes the code much cleaner to use the MathOp wrappers.
template <typename T, typename U, class Enable = void>
struct CheckedMinOp
{};

template <typename T, typename U>
struct CheckedMinOp<
    T,
    U,
    typename std::enable_if<std::is_arithmetic<T>::value && std::is_arithmetic<U>::value>::type>
{
    using result_type = typename LowestValuePromotion<T, U>::type;
    template <typename V>
    static constexpr bool Do(T x, U y, V *result)
    {
        const result_type tmp =
            IsLess<T, U>::Test(x, y) ? static_cast<result_type>(x) : static_cast<result_type>(y);
        if (!IsValueInRangeForNumericType<V>(tmp))
            return false;
        *result = static_cast<V>(tmp);
        return true;
    }
};

// This is just boilerplate that wraps the standard floating point arithmetic.
// A macro isn't the nicest solution, but it beats rewriting these repeatedly.
#define BASE_FLOAT_ARITHMETIC_OPS(NAME, OP)                                                   \
    template <typename T, typename U>                                                         \
    struct Checked##NAME##Op<T, U,                                                            \
                             typename std::enable_if<std::is_floating_point<T>::value ||      \
                                                     std::is_floating_point<U>::value>::type> \
    {                                                                                         \
        using result_type = typename MaxExponentPromotion<T, U>::type;                        \
        template <typename V>                                                                 \
        static constexpr bool Do(T x, U y, V *result)                                         \
        {                                                                                     \
            using Promotion         = typename MaxExponentPromotion<T, U>::type;              \
            const Promotion presult = x OP y;                                                 \
            if (!IsValueInRangeForNumericType<V>(presult))                                    \
                return false;                                                                 \
            *result = static_cast<V>(presult);                                                \
            return true;                                                                      \
        }                                                                                     \
    };

BASE_FLOAT_ARITHMETIC_OPS(Add, +)
BASE_FLOAT_ARITHMETIC_OPS(Sub, -)
BASE_FLOAT_ARITHMETIC_OPS(Mul, *)
BASE_FLOAT_ARITHMETIC_OPS(Div, /)

#undef BASE_FLOAT_ARITHMETIC_OPS

// Floats carry around their validity state with them, but integers do not. So,
// we wrap the underlying value in a specialization in order to hide that detail
// and expose an interface via accessors.
enum NumericRepresentation
{
    NUMERIC_INTEGER,
    NUMERIC_FLOATING,
    NUMERIC_UNKNOWN
};

template <typename NumericType>
struct GetNumericRepresentation
{
    static const NumericRepresentation value =
        std::is_integral<NumericType>::value
            ? NUMERIC_INTEGER
            : (std::is_floating_point<NumericType>::value ? NUMERIC_FLOATING : NUMERIC_UNKNOWN);
};

template <typename T, NumericRepresentation type = GetNumericRepresentation<T>::value>
class CheckedNumericState
{};

// Integrals require quite a bit of additional housekeeping to manage state.
template <typename T>
class CheckedNumericState<T, NUMERIC_INTEGER>
{
  public:
    template <typename Src = int>
    constexpr explicit CheckedNumericState(Src value = 0, bool is_valid = true)
        : is_valid_(is_valid && IsValueInRangeForNumericType<T>(value)),
          value_(WellDefinedConversionOrZero(value, is_valid_))
    {
        static_assert(std::is_arithmetic<Src>::value, "Argument must be numeric.");
    }

    template <typename Src>
    constexpr CheckedNumericState(const CheckedNumericState<Src> &rhs)
        : CheckedNumericState(rhs.value(), rhs.is_valid())
    {}

    constexpr bool is_valid() const { return is_valid_; }

    constexpr T value() const { return value_; }

  private:
    // Ensures that a type conversion does not trigger undefined behavior.
    template <typename Src>
    static constexpr T WellDefinedConversionOrZero(Src value, bool is_valid)
    {
        using SrcType = typename internal::UnderlyingType<Src>::type;
        return (std::is_integral<SrcType>::value || is_valid) ? static_cast<T>(value) : 0;
    }

    // is_valid_ precedes value_ because member intializers in the constructors
    // are evaluated in field order, and is_valid_ must be read when initializing
    // value_.
    bool is_valid_;
    T value_;
};

// Floating points maintain their own validity, but need translation wrappers.
template <typename T>
class CheckedNumericState<T, NUMERIC_FLOATING>
{
  public:
    template <typename Src = double>
    constexpr explicit CheckedNumericState(Src value = 0.0, bool is_valid = true)
        : value_(
              WellDefinedConversionOrNaN(value, is_valid && IsValueInRangeForNumericType<T>(value)))
    {}

    template <typename Src>
    constexpr CheckedNumericState(const CheckedNumericState<Src> &rhs)
        : CheckedNumericState(rhs.value(), rhs.is_valid())
    {}

    constexpr bool is_valid() const
    {
        // Written this way because std::isfinite is not reliably constexpr.
        return MustTreatAsConstexpr(value_) ? value_ <= std::numeric_limits<T>::max() &&
                                                  value_ >= std::numeric_limits<T>::lowest()
                                            : std::isfinite(value_);
    }

    constexpr T value() const { return value_; }

  private:
    // Ensures that a type conversion does not trigger undefined behavior.
    template <typename Src>
    static constexpr T WellDefinedConversionOrNaN(Src value, bool is_valid)
    {
        using SrcType = typename internal::UnderlyingType<Src>::type;
        return (StaticDstRangeRelationToSrcRange<T, SrcType>::value == NUMERIC_RANGE_CONTAINED ||
                is_valid)
                   ? static_cast<T>(value)
                   : std::numeric_limits<T>::quiet_NaN();
    }

    T value_;
};

}  // namespace internal
}  // namespace base
}  // namespace angle

#endif  // BASE_NUMERICS_CHECKED_MATH_IMPL_H_
