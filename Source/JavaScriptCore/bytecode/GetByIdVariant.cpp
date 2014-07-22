/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "GetByIdVariant.h"

#include "CallLinkStatus.h"
#include "JSCInlines.h"

namespace JSC {

GetByIdVariant::~GetByIdVariant() { }

GetByIdVariant::GetByIdVariant(const GetByIdVariant& other)
{
    *this = other;
}

GetByIdVariant& GetByIdVariant::operator=(const GetByIdVariant& other)
{
    m_structureSet = other.m_structureSet;
    m_chain = other.m_chain;
    m_specificValue = other.m_specificValue;
    m_offset = other.m_offset;
    if (other.m_callLinkStatus)
        m_callLinkStatus = std::make_unique<CallLinkStatus>(*other.m_callLinkStatus);
    else
        m_callLinkStatus = nullptr;
    return *this;
}

void GetByIdVariant::dump(PrintStream& out) const
{
    dumpInContext(out, 0);
}

void GetByIdVariant::dumpInContext(PrintStream& out, DumpContext* context) const
{
    if (!isSet()) {
        out.print("<empty>");
        return;
    }
    
    out.print(
        "<", inContext(structureSet(), context), ", ",
        pointerDumpInContext(chain(), context), ", ",
        inContext(specificValue(), context), ", ", offset());
    if (m_callLinkStatus)
        out.print("call: ", *m_callLinkStatus);
    out.print(">");
}

} // namespace JSC

