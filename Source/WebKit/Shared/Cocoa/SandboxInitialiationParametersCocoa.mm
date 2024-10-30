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

#import "config.h"
#import "SandboxInitializationParameters.h"

namespace WebKit {

SandboxInitializationParameters::SandboxInitializationParameters()
    : m_profileSelectionMode(ProfileSelectionMode::UseDefaultSandboxProfilePath)
{
}

SandboxInitializationParameters::~SandboxInitializationParameters() = default;

void SandboxInitializationParameters::appendPathInternal(ASCIILiteral name, const char* path)
{
    std::array<char, PATH_MAX> normalizedPath;
    if (!realpath(path, normalizedPath.data()))
        normalizedPath[0] = '\0';

    m_parameterNames.append(name);
    m_parameterValues.append(normalizedPath.data());
}

void SandboxInitializationParameters::addConfDirectoryParameter(ASCIILiteral name, int confID)
{
    std::array<char, PATH_MAX> path;
    if (confstr(confID, path.data(), PATH_MAX) <= 0)
        path[0] = '\0';

    appendPathInternal(name, path.data());
}

void SandboxInitializationParameters::addPathParameter(ASCIILiteral name, NSString *path)
{
    appendPathInternal(name, [path length] ? [(NSString *)path fileSystemRepresentation] : "");
}

void SandboxInitializationParameters::addPathParameter(ASCIILiteral name, const char* path)
{
    appendPathInternal(name, path);
}

void SandboxInitializationParameters::addParameter(ASCIILiteral name, CString&& value)
{
    m_parameterNames.append(name);
    m_parameterValues.append(WTFMove(value));
}

Vector<const char*> SandboxInitializationParameters::namedParameterVector() const
{
    Vector<const char*> result;
    result.reserveInitialCapacity(m_parameterNames.size() * 2 + 1);
    ASSERT(m_parameterNames.size() == m_parameterValues.size());
    for (size_t i = 0; i < m_parameterNames.size(); ++i) {
        result.append(m_parameterNames[i]);
        result.append(m_parameterValues[i].data());
    }
    result.append(nullptr);
    return result;
}

size_t SandboxInitializationParameters::count() const
{
    return m_parameterNames.size();
}

ASCIILiteral SandboxInitializationParameters::name(size_t index) const
{
    return m_parameterNames[index];
}

const char* SandboxInitializationParameters::value(size_t index) const
{
    return m_parameterValues[index].data();
}

} // namespace WebKit
