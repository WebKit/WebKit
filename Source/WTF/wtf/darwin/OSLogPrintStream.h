/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if OS(DARWIN)

#include <wtf/Lock.h>
#include <wtf/PrintStream.h>
#include <wtf/RecursiveLockAdapter.h>
#include <wtf/text/CString.h>
#include <wtf/Vector.h>

#include <os/log.h>

namespace WTF {

class WTF_EXPORT_PRIVATE OSLogPrintStream final : public PrintStream {
public:
    OSLogPrintStream(os_log_t, os_log_type_t);
    ~OSLogPrintStream() final;
    
    static std::unique_ptr<OSLogPrintStream> open(const char* subsystem, const char* category, os_log_type_t = OS_LOG_TYPE_DEFAULT);
    
    void vprintf(const char* format, va_list) final WTF_ATTRIBUTE_PRINTF(2, 0);

private:
    os_log_t m_log;
    os_log_type_t m_logType;
    Lock m_stringLock;
    // We need a buffer because os_log doesn't wait for a new line to print the characters.
    CString m_string WTF_GUARDED_BY_LOCK(m_stringLock);
    size_t m_offset { 0 };
};

} // namespace WTF

using WTF::OSLogPrintStream;

#endif
