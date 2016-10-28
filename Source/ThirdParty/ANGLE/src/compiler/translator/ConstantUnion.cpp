//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ConstantUnion: Constant folding helper class.

#include "compiler/translator/ConstantUnion.h"

#include "base/numerics/safe_math.h"
#include "common/mathutil.h"
#include "compiler/translator/Diagnostics.h"

namespace
{

template <typename T>
T CheckedSum(base::CheckedNumeric<T> lhs,
             base::CheckedNumeric<T> rhs,
             TDiagnostics *diag,
             const TSourceLoc &line)
{
    ASSERT(lhs.IsValid() && rhs.IsValid());
    auto result = lhs + rhs;
    if (!result.IsValid())
    {
        diag->error(line, "Addition out of range", "*", "");
        return 0;
    }
    return result.ValueOrDefault(0);
}

template <typename T>
T CheckedDiff(base::CheckedNumeric<T> lhs,
              base::CheckedNumeric<T> rhs,
              TDiagnostics *diag,
              const TSourceLoc &line)
{
    ASSERT(lhs.IsValid() && rhs.IsValid());
    auto result = lhs - rhs;
    if (!result.IsValid())
    {
        diag->error(line, "Difference out of range", "*", "");
        return 0;
    }
    return result.ValueOrDefault(0);
}

template <typename T>
T CheckedMul(base::CheckedNumeric<T> lhs,
             base::CheckedNumeric<T> rhs,
             TDiagnostics *diag,
             const TSourceLoc &line)
{
    ASSERT(lhs.IsValid() && rhs.IsValid());
    auto result = lhs * rhs;
    if (!result.IsValid())
    {
        diag->error(line, "Multiplication out of range", "*", "");
        return 0;
    }
    return result.ValueOrDefault(0);
}

}  // anonymous namespace

TConstantUnion::TConstantUnion()
{
    iConst = 0;
    type   = EbtVoid;
}

bool TConstantUnion::cast(TBasicType newType, const TConstantUnion &constant)
{
    switch (newType)
    {
        case EbtFloat:
            switch (constant.type)
            {
                case EbtInt:
                    setFConst(static_cast<float>(constant.getIConst()));
                    break;
                case EbtUInt:
                    setFConst(static_cast<float>(constant.getUConst()));
                    break;
                case EbtBool:
                    setFConst(static_cast<float>(constant.getBConst()));
                    break;
                case EbtFloat:
                    setFConst(static_cast<float>(constant.getFConst()));
                    break;
                default:
                    return false;
            }
            break;
        case EbtInt:
            switch (constant.type)
            {
                case EbtInt:
                    setIConst(static_cast<int>(constant.getIConst()));
                    break;
                case EbtUInt:
                    setIConst(static_cast<int>(constant.getUConst()));
                    break;
                case EbtBool:
                    setIConst(static_cast<int>(constant.getBConst()));
                    break;
                case EbtFloat:
                    setIConst(static_cast<int>(constant.getFConst()));
                    break;
                default:
                    return false;
            }
            break;
        case EbtUInt:
            switch (constant.type)
            {
                case EbtInt:
                    setUConst(static_cast<unsigned int>(constant.getIConst()));
                    break;
                case EbtUInt:
                    setUConst(static_cast<unsigned int>(constant.getUConst()));
                    break;
                case EbtBool:
                    setUConst(static_cast<unsigned int>(constant.getBConst()));
                    break;
                case EbtFloat:
                    setUConst(static_cast<unsigned int>(constant.getFConst()));
                    break;
                default:
                    return false;
            }
            break;
        case EbtBool:
            switch (constant.type)
            {
                case EbtInt:
                    setBConst(constant.getIConst() != 0);
                    break;
                case EbtUInt:
                    setBConst(constant.getUConst() != 0);
                    break;
                case EbtBool:
                    setBConst(constant.getBConst());
                    break;
                case EbtFloat:
                    setBConst(constant.getFConst() != 0.0f);
                    break;
                default:
                    return false;
            }
            break;
        case EbtStruct:  // Struct fields don't get cast
            switch (constant.type)
            {
                case EbtInt:
                    setIConst(constant.getIConst());
                    break;
                case EbtUInt:
                    setUConst(constant.getUConst());
                    break;
                case EbtBool:
                    setBConst(constant.getBConst());
                    break;
                case EbtFloat:
                    setFConst(constant.getFConst());
                    break;
                default:
                    return false;
            }
            break;
        default:
            return false;
    }

    return true;
}

bool TConstantUnion::operator==(const int i) const
{
    return i == iConst;
}

bool TConstantUnion::operator==(const unsigned int u) const
{
    return u == uConst;
}

bool TConstantUnion::operator==(const float f) const
{
    return f == fConst;
}

bool TConstantUnion::operator==(const bool b) const
{
    return b == bConst;
}

bool TConstantUnion::operator==(const TConstantUnion &constant) const
{
    if (constant.type != type)
        return false;

    switch (type)
    {
        case EbtInt:
            return constant.iConst == iConst;
        case EbtUInt:
            return constant.uConst == uConst;
        case EbtFloat:
            return constant.fConst == fConst;
        case EbtBool:
            return constant.bConst == bConst;
        default:
            return false;
    }
}

bool TConstantUnion::operator!=(const int i) const
{
    return !operator==(i);
}

bool TConstantUnion::operator!=(const unsigned int u) const
{
    return !operator==(u);
}

bool TConstantUnion::operator!=(const float f) const
{
    return !operator==(f);
}

bool TConstantUnion::operator!=(const bool b) const
{
    return !operator==(b);
}

bool TConstantUnion::operator!=(const TConstantUnion &constant) const
{
    return !operator==(constant);
}

bool TConstantUnion::operator>(const TConstantUnion &constant) const
{
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            return iConst > constant.iConst;
        case EbtUInt:
            return uConst > constant.uConst;
        case EbtFloat:
            return fConst > constant.fConst;
        default:
            return false;  // Invalid operation, handled at semantic analysis
    }
}

bool TConstantUnion::operator<(const TConstantUnion &constant) const
{
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            return iConst < constant.iConst;
        case EbtUInt:
            return uConst < constant.uConst;
        case EbtFloat:
            return fConst < constant.fConst;
        default:
            return false;  // Invalid operation, handled at semantic analysis
    }
}

// static
TConstantUnion TConstantUnion::add(const TConstantUnion &lhs,
                                   const TConstantUnion &rhs,
                                   TDiagnostics *diag,
                                   const TSourceLoc &line)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == rhs.type);
    switch (lhs.type)
    {
        case EbtInt:
            returnValue.setIConst(gl::WrappingSum<int>(lhs.iConst, rhs.iConst));
            break;
        case EbtUInt:
            returnValue.setUConst(gl::WrappingSum<unsigned int>(lhs.uConst, rhs.uConst));
            break;
        case EbtFloat:
            returnValue.setFConst(CheckedSum<float>(lhs.fConst, rhs.fConst, diag, line));
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

// static
TConstantUnion TConstantUnion::sub(const TConstantUnion &lhs,
                                   const TConstantUnion &rhs,
                                   TDiagnostics *diag,
                                   const TSourceLoc &line)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == rhs.type);
    switch (lhs.type)
    {
        case EbtInt:
            returnValue.setIConst(gl::WrappingDiff<int>(lhs.iConst, rhs.iConst));
            break;
        case EbtUInt:
            returnValue.setUConst(gl::WrappingDiff<unsigned int>(lhs.uConst, rhs.uConst));
            break;
        case EbtFloat:
            returnValue.setFConst(CheckedDiff<float>(lhs.fConst, rhs.fConst, diag, line));
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

// static
TConstantUnion TConstantUnion::mul(const TConstantUnion &lhs,
                                   const TConstantUnion &rhs,
                                   TDiagnostics *diag,
                                   const TSourceLoc &line)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == rhs.type);
    switch (lhs.type)
    {
        case EbtInt:
            returnValue.setIConst(gl::WrappingMul(lhs.iConst, rhs.iConst));
            break;
        case EbtUInt:
            // Unsigned integer math in C++ is defined to be done in modulo 2^n, so we rely on that
            // to implement wrapping multiplication.
            returnValue.setUConst(lhs.uConst * rhs.uConst);
            break;
        case EbtFloat:
            returnValue.setFConst(CheckedMul<float>(lhs.fConst, rhs.fConst, diag, line));
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator%(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst % constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst % constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

// static
TConstantUnion TConstantUnion::rshift(const TConstantUnion &lhs,
                                      const TConstantUnion &rhs,
                                      TDiagnostics *diag,
                                      const TSourceLoc &line)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == EbtInt || lhs.type == EbtUInt);
    ASSERT(rhs.type == EbtInt || rhs.type == EbtUInt);
    if ((rhs.type == EbtInt && (rhs.iConst < 0 || rhs.iConst > 31)) ||
        (rhs.type == EbtUInt && rhs.uConst > 31u))
    {
        diag->error(line, "Undefined shift (operand out of range)", ">>", "");
        switch (lhs.type)
        {
            case EbtInt:
                returnValue.setIConst(0);
                break;
            case EbtUInt:
                returnValue.setUConst(0u);
                break;
            default:
                UNREACHABLE();
        }
        return returnValue;
    }

    switch (lhs.type)
    {
        case EbtInt:
        {
            unsigned int shiftOffset = 0;
            switch (rhs.type)
            {
                case EbtInt:
                    shiftOffset = static_cast<unsigned int>(rhs.iConst);
                    break;
                case EbtUInt:
                    shiftOffset = rhs.uConst;
                    break;
                default:
                    UNREACHABLE();
            }
            if (shiftOffset > 0)
            {
                // ESSL 3.00.6 section 5.9: "If E1 is a signed integer, the right-shift will extend
                // the sign bit." In C++ shifting negative integers is undefined, so we implement
                // extending the sign bit manually.
                bool extendSignBit = false;
                int lhsSafe        = lhs.iConst;
                if (lhsSafe < 0)
                {
                    extendSignBit = true;
                    // Clear the sign bit so that bitshift right is defined in C++.
                    lhsSafe &= 0x7fffffff;
                    ASSERT(lhsSafe > 0);
                }
                returnValue.setIConst(lhsSafe >> shiftOffset);

                // Manually fill in the extended sign bit if necessary.
                if (extendSignBit)
                {
                    int extendedSignBit = static_cast<int>(0xffffffffu << (31 - shiftOffset));
                    returnValue.setIConst(returnValue.getIConst() | extendedSignBit);
                }
            }
            else
            {
                returnValue.setIConst(rhs.iConst);
            }
            break;
        }
        case EbtUInt:
            switch (rhs.type)
            {
                case EbtInt:
                    returnValue.setUConst(lhs.uConst >> rhs.iConst);
                    break;
                case EbtUInt:
                    returnValue.setUConst(lhs.uConst >> rhs.uConst);
                    break;
                default:
                    UNREACHABLE();
            }
            break;

        default:
            UNREACHABLE();
    }
    return returnValue;
}

// static
TConstantUnion TConstantUnion::lshift(const TConstantUnion &lhs,
                                      const TConstantUnion &rhs,
                                      TDiagnostics *diag,
                                      const TSourceLoc &line)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == EbtInt || lhs.type == EbtUInt);
    ASSERT(rhs.type == EbtInt || rhs.type == EbtUInt);
    if ((rhs.type == EbtInt && (rhs.iConst < 0 || rhs.iConst > 31)) ||
        (rhs.type == EbtUInt && rhs.uConst > 31u))
    {
        diag->error(line, "Undefined shift (operand out of range)", "<<", "");
        switch (lhs.type)
        {
            case EbtInt:
                returnValue.setIConst(0);
                break;
            case EbtUInt:
                returnValue.setUConst(0u);
                break;
            default:
                UNREACHABLE();
        }
        return returnValue;
    }

    switch (lhs.type)
    {
        case EbtInt:
            switch (rhs.type)
            {
                // Cast to unsigned integer before shifting, since ESSL 3.00.6 section 5.9 says that
                // lhs is "interpreted as a bit pattern". This also avoids the possibility of signed
                // integer overflow or undefined shift of a negative integer.
                case EbtInt:
                    returnValue.setIConst(
                        static_cast<int>(static_cast<uint32_t>(lhs.iConst) << rhs.iConst));
                    break;
                case EbtUInt:
                    returnValue.setIConst(
                        static_cast<int>(static_cast<uint32_t>(lhs.iConst) << rhs.uConst));
                    break;
                default:
                    UNREACHABLE();
            }
            break;

        case EbtUInt:
            switch (rhs.type)
            {
                case EbtInt:
                    returnValue.setUConst(lhs.uConst << rhs.iConst);
                    break;
                case EbtUInt:
                    returnValue.setUConst(lhs.uConst << rhs.uConst);
                    break;
                default:
                    UNREACHABLE();
            }
            break;

        default:
            UNREACHABLE();
    }
    return returnValue;
}

TConstantUnion TConstantUnion::operator&(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(constant.type == EbtInt || constant.type == EbtUInt);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst & constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst & constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator|(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst | constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst | constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator^(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst ^ constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst ^ constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator&&(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtBool:
            returnValue.setBConst(bConst && constant.bConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator||(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtBool:
            returnValue.setBConst(bConst || constant.bConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}
