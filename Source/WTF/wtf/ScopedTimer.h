/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/Logger.h>
#include <wtf/MonotonicTime.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class ScopedTimer {
public:
    ScopedTimer()
        : m_name("ScopedTimer"_s)
    {
    }
    
    ScopedTimer(ASCIILiteral name)
        : m_name(name)
    {
    }

    ScopedTimer(const Logger::LogSiteIdentifier& logIdentifier)
        : m_name(logIdentifier.toString().stripWhiteSpace())
    {
    }
    
    template<typename... Arguments>
    ScopedTimer(UNUSED_FUNCTION const Arguments&... arguments)
        : m_name(makeString(LogArgument<Arguments>::toString(arguments)...))
    {
    }
    
    WTF_EXPORT_PRIVATE ~ScopedTimer();
    
#define WTFScopedTimer(...) ScopedTimer __SCOPED_TIMER__(__VA_ARGS__)

private:
    String m_name;
    MonotonicTime m_startTime { MonotonicTime::now() };
};

} // namespace WTF

using WTF::ScopedTimer;
