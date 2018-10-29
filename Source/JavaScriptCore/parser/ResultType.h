/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

namespace JSC {

    struct ResultType {
    private:
        friend struct OperandTypes;

        using Type = uint8_t;
        static constexpr Type TypeInt32 = 1;
        static constexpr Type TypeMaybeNumber = 0x02;
        static constexpr Type TypeMaybeString = 0x04;
        static constexpr Type TypeMaybeNull   = 0x08;
        static constexpr Type TypeMaybeBool   = 0x10;
        static constexpr Type TypeMaybeBigInt = 0x20;
        static constexpr Type TypeMaybeOther  = 0x40;

        static constexpr Type TypeBits = TypeMaybeNumber | TypeMaybeString | TypeMaybeNull | TypeMaybeBool | TypeMaybeBigInt | TypeMaybeOther;

    public:
        static constexpr int numBitsNeeded = 7;
        static_assert((TypeBits & ((1 << numBitsNeeded) - 1)) == TypeBits, "This is necessary for correctness.");

        constexpr explicit ResultType(Type type)
            : m_bits(type)
        {
        }

        constexpr bool isInt32() const
        {
            return m_bits & TypeInt32;
        }

        constexpr bool definitelyIsNumber() const
        {
            return (m_bits & TypeBits) == TypeMaybeNumber;
        }
        
        constexpr bool definitelyIsString() const
        {
            return (m_bits & TypeBits) == TypeMaybeString;
        }

        constexpr bool definitelyIsBoolean() const
        {
            return (m_bits & TypeBits) == TypeMaybeBool;
        }

        constexpr bool definitelyIsBigInt() const
        {
            return (m_bits & TypeBits) == TypeMaybeBigInt;
        }

        constexpr bool mightBeNumber() const
        {
            return m_bits & TypeMaybeNumber;
        }

        constexpr bool isNotNumber() const
        {
            return !mightBeNumber();
        }
        
        constexpr bool mightBeBigInt() const
        {
            return m_bits & TypeMaybeBigInt;
        }

        constexpr bool isNotBigInt() const
        {
            return !mightBeBigInt();
        }
        
        static constexpr ResultType nullType()
        {
            return ResultType(TypeMaybeNull);
        }
        
        static constexpr ResultType booleanType()
        {
            return ResultType(TypeMaybeBool);
        }
        
        static constexpr ResultType numberType()
        {
            return ResultType(TypeMaybeNumber);
        }
        
        static constexpr ResultType numberTypeIsInt32()
        {
            return ResultType(TypeInt32 | TypeMaybeNumber);
        }
        
        static constexpr ResultType stringOrNumberType()
        {
            return ResultType(TypeMaybeNumber | TypeMaybeString);
        }
        
        static constexpr ResultType addResultType()
        {
            return ResultType(TypeMaybeNumber | TypeMaybeString | TypeMaybeBigInt);
        }
        
        static constexpr ResultType stringType()
        {
            return ResultType(TypeMaybeString);
        }
        
        static constexpr ResultType bigIntType()
        {
            return ResultType(TypeMaybeBigInt);
        }
        
        static constexpr ResultType unknownType()
        {
            return ResultType(TypeBits);
        }
        
        static constexpr ResultType forAdd(ResultType op1, ResultType op2)
        {
            if (op1.definitelyIsNumber() && op2.definitelyIsNumber())
                return numberType();
            if (op1.definitelyIsString() || op2.definitelyIsString())
                return stringType();
            if (op1.definitelyIsBigInt() && op2.definitelyIsBigInt())
                return bigIntType();
            return addResultType();
        }

        // Unlike in C, a logical op produces the value of the
        // last expression evaluated (and not true or false).
        static constexpr ResultType forLogicalOp(ResultType op1, ResultType op2)
        {
            if (op1.definitelyIsBoolean() && op2.definitelyIsBoolean())
                return booleanType();
            if (op1.definitelyIsNumber() && op2.definitelyIsNumber())
                return numberType();
            if (op1.definitelyIsString() && op2.definitelyIsString())
                return stringType();
            if (op1.definitelyIsBigInt() && op2.definitelyIsBigInt())
                return bigIntType();
            return unknownType();
        }

        static constexpr ResultType forBitOp()
        {
            return numberTypeIsInt32();
        }

        constexpr Type bits() const { return m_bits; }

        void dump(PrintStream& out) const
        {
            // FIXME: more meaningful information
            // https://bugs.webkit.org/show_bug.cgi?id=190930
            out.print(bits());
        }

    private:
        Type m_bits;
    };
    
    struct OperandTypes
    {
        OperandTypes(ResultType first = ResultType::unknownType(), ResultType second = ResultType::unknownType())
        {
            // We have to initialize one of the int to ensure that
            // the entire struct is initialized.
            m_u.i = 0;
            m_u.rds.first = first.m_bits;
            m_u.rds.second = second.m_bits;
        }
        
        union {
            struct {
                ResultType::Type first;
                ResultType::Type second;
            } rds;
            int i;
        } m_u;

        ResultType first() const
        {
            return ResultType(m_u.rds.first);
        }

        ResultType second() const
        {
            return ResultType(m_u.rds.second);
        }

        int toInt()
        {
            return m_u.i;
        }
        static OperandTypes fromInt(int value)
        {
            OperandTypes types;
            types.m_u.i = value;
            return types;
        }

        void dump(PrintStream& out) const
        {
            out.print("OperandTypes(", first(),  ", ", second(), ")");
        }
    };

} // namespace JSC
