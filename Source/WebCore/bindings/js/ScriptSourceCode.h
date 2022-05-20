/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AbstractScriptSourceCode.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

class ScriptSourceCode : public AbstractScriptSourceCode {
public:
    ScriptSourceCode(const String& source, URL&& url = URL(), const TextPosition& startPosition = TextPosition(), JSC::SourceProviderSourceType sourceType = JSC::SourceProviderSourceType::Program)
        : AbstractScriptSourceCode(JSC::StringSourceProvider::create(source, JSC::SourceOrigin { url }, url.string(), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt())
    {
    }

    ScriptSourceCode(const ScriptBuffer& source, URL&& url = URL(), const TextPosition& startPosition = TextPosition(), JSC::SourceProviderSourceType sourceType = JSC::SourceProviderSourceType::Program)
        : AbstractScriptSourceCode(ScriptBufferSourceProvider::create(source, JSC::SourceOrigin { url }, url.string(), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt())
    {
    }

    ScriptSourceCode(CachedScript* cachedScript, JSC::SourceProviderSourceType sourceType, Ref<CachedScriptFetcher>&& scriptFetcher)
        : AbstractScriptSourceCode(CachedScriptSourceProvider::create(cachedScript, sourceType, WTFMove(scriptFetcher)), cachedScript)
    {
    }

    ScriptSourceCode(const String& source, URL&& url, const TextPosition& startPosition, JSC::SourceProviderSourceType sourceType, Ref<JSC::ScriptFetcher>&& scriptFetcher)
        : AbstractScriptSourceCode(JSC::StringSourceProvider::create(source, JSC::SourceOrigin { url, WTFMove(scriptFetcher) }, url.string(), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt())
    {
    }

    ScriptSourceCode(const ScriptBuffer& source, URL&& url, const TextPosition& startPosition, JSC::SourceProviderSourceType sourceType, Ref<JSC::ScriptFetcher>&& scriptFetcher)
        : AbstractScriptSourceCode(ScriptBufferSourceProvider::create(source, JSC::SourceOrigin { url, WTFMove(scriptFetcher) }, url.string(), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt())
    {
    }
};

} // namespace WebCore
