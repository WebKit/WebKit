/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SandboxInitializationParameters_h
#define SandboxInitializationParameters_h

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class SandboxInitializationParameters {
WTF_MAKE_NONCOPYABLE(SandboxInitializationParameters);
public:

#if PLATFORM(MAC)
    SandboxInitializationParameters();
    ~SandboxInitializationParameters();

    // Name must be a literal.
    void addConfDirectoryParameter(const char* name, int confID);
    void addPathParameter(const char* name, NSString *path);
    void addPathParameter(const char* name, const char* path);

    const char* const* namedParameterArray() const;

    size_t count() const;
    const char* name(size_t index) const;
    const char* value(size_t index) const;

    void setSandboxProfilePath(const String& path) { m_sandboxProfilePath = path; m_sandboxProfile = String(); }
    const String& sandboxProfilePath() const { return m_sandboxProfilePath; }

    void setSandboxProfile(const String& profile) { m_sandboxProfilePath = String(); m_sandboxProfile = profile; }
    const String& sandboxProfile() const { return m_sandboxProfile; }

    void setSystemDirectorySuffix(const String& suffix) { m_systemDirectorySuffix = suffix; }
    const String& systemDirectorySuffix() const { return m_systemDirectorySuffix; }
#endif

private:
#if PLATFORM(MAC)
    void appendPathInternal(const char* name, const char* path);

    mutable Vector<const char*> m_namedParameters;
    String m_systemDirectorySuffix;

    String m_sandboxProfilePath;
    String m_sandboxProfile;
#endif
};

}

#endif // SandboxInitializationParameters_h
