/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "ProfilerBytecodes.h"

#include "CodeBlock.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "ProfilerDumper.h"
#include <wtf/StringPrintStream.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/CharacterNames.h>

namespace JSC { namespace Profiler {

Bytecodes::Bytecodes(size_t id, CodeBlock* codeBlock)
    : BytecodeSequence(codeBlock)
    , m_id(id)
    , m_inferredName(codeBlock->inferredName())
    , m_sourceCode(codeBlock->sourceCodeForTools())
    , m_hash(codeBlock->hash())
    , m_instructionCount(codeBlock->instructionsSize())
{
}

Bytecodes::~Bytecodes() = default;

void Bytecodes::dump(PrintStream& out) const
{
    out.print("#", m_hash, "(", m_id, ")");
}

Ref<JSON::Value> Bytecodes::toJSON(Dumper& dumper) const
{
    auto result = JSON::Object::create();

    result->setDouble(dumper.keys().m_bytecodesID, m_id);
    result->setString(dumper.keys().m_inferredName, String::fromUTF8(m_inferredName.span()));
    String sourceCode = String::fromUTF8(m_sourceCode.span());
    if (Options::abbreviateSourceCodeForProfiler()) {
        unsigned size = Options::abbreviateSourceCodeForProfiler();
        if (sourceCode.length() > size)
            sourceCode = makeString(StringView(sourceCode).left(size - 1), horizontalEllipsis);
    }
    result->setString(dumper.keys().m_sourceCode, WTFMove(sourceCode));
    result->setString(dumper.keys().m_hash, String::fromUTF8(toCString(m_hash).span()));
    result->setDouble(dumper.keys().m_instructionCount, m_instructionCount);
    addSequenceProperties(dumper, result.get());

    return result;
}

} } // namespace JSC::Profiler

