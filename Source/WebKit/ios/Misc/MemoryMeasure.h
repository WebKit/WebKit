/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef MemoryMeasure_h
#define MemoryMeasure_h

namespace WebKit {

/*!
 * A simple class that measures the difference in the resident memory of the
 * process between its contruction and destruction. It uses the mach API -
 * task_info - to figure this out.
 */
class MemoryMeasure {
public:
    MemoryMeasure()
        : m_logString("")
        , m_initialMemory(taskMemory())
    {
    }

    MemoryMeasure(const char *log)
        : m_logString(log)
        , m_initialMemory(taskMemory())
    {
    }

    ~MemoryMeasure();

    static void enableLogging(bool enabled);
    static bool isLoggingEnabled();

private:
    const char *m_logString;
    long m_initialMemory;
    static bool m_isLoggingEnabled;

    /*!
     * @return The resident memory (in bytes) consumed by the process. It uses
     *         task_info() to get this information. If there is an error, it
     *         returns -1.
     */
    long taskMemory();
};

}
#endif
