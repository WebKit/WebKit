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

#if PLATFORM(COCOA)
OBJC_CLASS NSString;
#endif

namespace WebKit {

class SandboxInitializationParameters {
    WTF_MAKE_NONCOPYABLE(SandboxInitializationParameters);
public:
    SandboxInitializationParameters();
    ~SandboxInitializationParameters();

#if PLATFORM(COCOA)
    // Name must be a literal.
    void addConfDirectoryParameter(const char* name, int confID);
    void addPathParameter(const char* name, NSString *path);
    void addPathParameter(const char* name, const char* path);
    void addParameter(const char* name, const char* value);

    const char* const* namedParameterArray() const;

    size_t count() const;
    const char* name(size_t index) const;
    const char* value(size_t index) const;

    enum class ProfileSelectionMode : uint8_t {
        UseDefaultSandboxProfilePath,
        UseOverrideSandboxProfilePath,
        UseSandboxProfile
    };

    ProfileSelectionMode mode() const { return m_profileSelectionMode; }

    void setOverrideSandboxProfilePath(const String& path)
    {
        m_profileSelectionMode = ProfileSelectionMode::UseOverrideSandboxProfilePath;
        m_overrideSandboxProfilePathOrSandboxProfile = path;
    }

    const String& overrideSandboxProfilePath() const
    {
        ASSERT(m_profileSelectionMode == ProfileSelectionMode::UseOverrideSandboxProfilePath);
        return m_overrideSandboxProfilePathOrSandboxProfile;
    }

    void setSandboxProfile(const String& profile)
    {
        m_profileSelectionMode = ProfileSelectionMode::UseSandboxProfile;
        m_overrideSandboxProfilePathOrSandboxProfile = profile;
    }

    const String& sandboxProfile() const
    {
        ASSERT(m_profileSelectionMode == ProfileSelectionMode::UseSandboxProfile);
        return m_overrideSandboxProfilePathOrSandboxProfile;
    }

    void setUserDirectorySuffix(const String& suffix) { m_userDirectorySuffix = suffix; }
    const String& userDirectorySuffix() const { return m_userDirectorySuffix; }
#endif

private:
#if PLATFORM(COCOA)
    void appendPathInternal(const char* name, const char* path);

    mutable Vector<const char*> m_namedParameters;
    String m_userDirectorySuffix;

    ProfileSelectionMode m_profileSelectionMode;
    String m_overrideSandboxProfilePathOrSandboxProfile;
#endif
};

#if !PLATFORM(COCOA)
SandboxInitializationParameters::SandboxInitializationParameters()
{
}

SandboxInitializationParameters::~SandboxInitializationParameters()
{
}
#endif

} // namespace WebKit

#endif // SandboxInitializationParameters_h
