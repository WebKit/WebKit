/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "ProfilerEvent.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "ProfilerBytecodes.h"
#include "ProfilerCompilation.h"
#include "ProfilerDumper.h"
#include "ProfilerUID.h"

namespace JSC { namespace Profiler {

void Event::dump(PrintStream& out) const
{
    out.print(m_time, ": ", pointerDump(m_bytecodes));
    if (m_compilation)
        out.print(" ", *m_compilation);
    out.print(": ", m_summary);
    if (m_detail.length())
        out.print(" (", m_detail, ")");
}

Ref<JSON::Value> Event::toJSON(Dumper& dumper) const
{
    auto result = JSON::Object::create();

    result->setDouble(dumper.keys().m_time, m_time.secondsSinceEpoch().value());
    result->setDouble(dumper.keys().m_bytecodesID, m_bytecodes->id());
    if (m_compilation)
        result->setValue(dumper.keys().m_compilationUID, m_compilation->uid().toJSON(dumper));
    result->setString(dumper.keys().m_summary, String::fromUTF8(m_summary));
    if (m_detail.length())
        result->setString(dumper.keys().m_detail, String::fromUTF8(m_detail.span()));

    return result;
}

} } // namespace JSC::Profiler

