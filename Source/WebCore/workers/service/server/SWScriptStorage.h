/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <wtf/FileSystem.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ServiceWorkerRegistrationKey;
class ScriptBuffer;

class SWScriptStorage {
    WTF_MAKE_TZONE_ALLOCATED(SWScriptStorage);
public:
    explicit SWScriptStorage(StringImpl* directory);

    ScriptBuffer store(const ServiceWorkerRegistrationKey&, const URL& scriptURL, const ScriptBuffer&);
    ScriptBuffer retrieve(const ServiceWorkerRegistrationKey&, const URL& scriptURL);
    void clear(const ServiceWorkerRegistrationKey&);

private:
    RefPtr<StringImpl> registrationDirectory(const ServiceWorkerRegistrationKey&) const;
    RefPtr<StringImpl> scriptPath(const ServiceWorkerRegistrationKey&, const URL& scriptURL) const;
    RefPtr<StringImpl> saltPath() const;
    RefPtr<StringImpl> sha2Hash(StringImpl*) const;
    RefPtr<StringImpl> sha2Hash(const URL&) const;

    RefPtr<StringImpl> m_directory;
    FileSystem::Salt m_salt;
};

} // namespace WebCore
