/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AirArg.h"

#if ENABLE(B3_JIT)

#include "AirSpecial.h"
#include "AirStackSlot.h"

namespace JSC { namespace B3 { namespace Air {

bool Arg::isRepresentableAs(Width width, Signedness signedness) const
{
    switch (signedness) {
    case Signed:
        switch (width) {
        case Width8:
            return isRepresentableAs<int8_t>();
        case Width16:
            return isRepresentableAs<int16_t>();
        case Width32:
            return isRepresentableAs<int32_t>();
        case Width64:
            return isRepresentableAs<int64_t>();
        }
    case Unsigned:
        switch (width) {
        case Width8:
            return isRepresentableAs<uint8_t>();
        case Width16:
            return isRepresentableAs<uint16_t>();
        case Width32:
            return isRepresentableAs<uint32_t>();
        case Width64:
            return isRepresentableAs<uint64_t>();
        }
    }
}

void Arg::dump(PrintStream& out) const
{
    switch (m_kind) {
    case Invalid:
        out.print("<invalid>");
        return;
    case Tmp:
        out.print(tmp());
        return;
    case Imm:
        out.print("$", m_offset);
        return;
    case Imm64:
        out.printf("$0x%llx", static_cast<long long unsigned>(m_offset));
        return;
    case Addr:
        if (offset())
            out.print(offset());
        out.print("(", base(), ")");
        return;
    case Index:
        if (offset())
            out.print(offset());
        out.print("(", base(), ",", index());
        if (scale() != 1)
            out.print(",", scale());
        out.print(")");
        return;
    case Stack:
        if (offset())
            out.print(offset());
        out.print("(", pointerDump(stackSlot()), ")");
        return;
    case CallArg:
        if (offset())
            out.print(offset());
        out.print("(callArg)");
        return;
    case RelCond:
        out.print(asRelationalCondition());
        return;
    case ResCond:
        out.print(asResultCondition());
        return;
    case DoubleCond:
        out.print(asDoubleCondition());
        return;
    case Special:
        out.print(pointerDump(special()));
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} } } // namespace JSC::B3::Air

namespace WTF {

using namespace JSC::B3::Air;

void printInternal(PrintStream& out, Arg::Kind kind)
{
    switch (kind) {
    case Arg::Invalid:
        out.print("Invalid");
        return;
    case Arg::Tmp:
        out.print("Tmp");
        return;
    case Arg::Imm:
        out.print("Imm");
        return;
    case Arg::Imm64:
        out.print("Imm64");
        return;
    case Arg::Addr:
        out.print("Addr");
        return;
    case Arg::Stack:
        out.print("Stack");
        return;
    case Arg::CallArg:
        out.print("CallArg");
        return;
    case Arg::Index:
        out.print("Index");
        return;
    case Arg::RelCond:
        out.print("RelCond");
        return;
    case Arg::ResCond:
        out.print("ResCond");
        return;
    case Arg::DoubleCond:
        out.print("DoubleCond");
        return;
    case Arg::Special:
        out.print("Special");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, Arg::Role role)
{
    switch (role) {
    case Arg::Use:
        out.print("Use");
        return;
    case Arg::Def:
        out.print("Def");
        return;
    case Arg::UseDef:
        out.print("UseDef");
        return;
    case Arg::ZDef:
        out.print("ZDef");
        return;
    case Arg::UseZDef:
        out.print("UseZDef");
        return;
    case Arg::UseAddr:
        out.print("UseAddr");
        return;
    case Arg::ColdUse:
        out.print("ColdUse");
        return;
    case Arg::LateUse:
        out.print("LateUse");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, Arg::Type type)
{
    switch (type) {
    case Arg::GP:
        out.print("GP");
        return;
    case Arg::FP:
        out.print("FP");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, Arg::Width width)
{
    switch (width) {
    case Arg::Width8:
        out.print("Width8");
        return;
    case Arg::Width16:
        out.print("Width16");
        return;
    case Arg::Width32:
        out.print("Width32");
        return;
    case Arg::Width64:
        out.print("Width64");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, Arg::Signedness signedness)
{
    switch (signedness) {
    case Arg::Signed:
        out.print("Signed");
        return;
    case Arg::Unsigned:
        out.print("Unsigned");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(B3_JIT)
