/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "ScriptSourceCode.h"

#include "CachedMetadata.h"
#include "CachedScript.h"
#include "V8Binding.h"

namespace WebCore {

PassOwnPtr<v8::ScriptData> ScriptSourceCode::precompileScript(v8::Handle<v8::String> code, CachedScript* cachedScript)
{
    // A pseudo-randomly chosen ID used to store and retrieve V8 ScriptData from
    // the CachedScript. If the format changes, this ID should be changed too.
    static const unsigned dataTypeID = 0xECC13BD7;

    // Very small scripts are not worth the effort to preparse.
    static const int minPreparseLength = 1024;

    if (!cachedScript || code->Length() < minPreparseLength)
        return nullptr;

    CachedMetadata* cachedMetadata = cachedScript->cachedMetadata(dataTypeID);
    if (cachedMetadata)
        return adoptPtr(v8::ScriptData::New(cachedMetadata->data(), cachedMetadata->size()));

    OwnPtr<v8::ScriptData> scriptData = adoptPtr(v8::ScriptData::PreCompile(code));
    cachedScript->setCachedMetadata(dataTypeID, scriptData->Data(), scriptData->Length());

    return scriptData.release();
}

v8::Handle<v8::Script> ScriptSourceCode::compileScript(v8::Handle<v8::String> code, const String& fileName, const TextPosition& scriptStartPosition, v8::ScriptData* scriptData)
{
    v8::Handle<v8::String> name = v8String(fileName);
    v8::Handle<v8::Integer> line = v8Integer(scriptStartPosition.m_line.zeroBasedInt());
    v8::Handle<v8::Integer> column = v8Integer(scriptStartPosition.m_column.zeroBasedInt());
    v8::ScriptOrigin origin(name, line, column);
    v8::Handle<v8::Script> script = v8::Script::Compile(code, &origin, scriptData);
    return script;
}

} // namespace WebCore
