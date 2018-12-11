/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "WebMemorySampler.h"

#if ENABLE(MEMORY_SAMPLER)

#include <stdio.h>
#include <wtf/ProcessID.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

static const char separator = '\t';

static void appendSpaces(StringBuilder& string, int count)
{
    for (int i = 0; i < count; ++i)
        string.append(' ');
}

WebMemorySampler* WebMemorySampler::singleton()
{
    static WebMemorySampler* sharedMemorySampler;
    if (!sharedMemorySampler)
        sharedMemorySampler = new WebMemorySampler();
    return sharedMemorySampler;
}

WebMemorySampler::WebMemorySampler() 
    : m_sampleTimer(*this, &WebMemorySampler::sampleTimerFired)
    , m_stopTimer(*this, &WebMemorySampler::stopTimerFired)
    , m_isRunning(false)
    , m_runningTime(0)
{
}

void WebMemorySampler::start(const double interval) 
{
    if (m_isRunning) 
        return;
    
    initializeTempLogFile();
    initializeTimers(interval);
}

void WebMemorySampler::start(SandboxExtension::Handle&& sampleLogFileHandle, const String& sampleLogFilePath, const double interval)
{
    if (m_isRunning) 
        return;
    
    // If we are on a system without SandboxExtension the handle and filename will be empty
    if (sampleLogFilePath.isEmpty()) {
        start(interval);
        return;
    }
        
    initializeSandboxedLogFile(WTFMove(sampleLogFileHandle), sampleLogFilePath);
    initializeTimers(interval);
   
}

void WebMemorySampler::initializeTimers(double interval)
{
    m_sampleTimer.startRepeating(1_s);
    printf("Started memory sampler for process %s %d", processName().utf8().data(), getCurrentProcessID());
    if (interval > 0) {
        m_stopTimer.startOneShot(1_s * interval);
        printf(" for a interval of %g seconds", interval);
    }
    printf("; Sampler log file stored at: %s\n", m_sampleLogFilePath.utf8().data());
    m_runningTime = interval;
    m_isRunning = true;
}

void WebMemorySampler::stop() 
{
    if (!m_isRunning) 
        return;
    m_sampleTimer.stop();
    FileSystem::closeFile(m_sampleLogFile);

    printf("Stopped memory sampler for process %s %d\n", processName().utf8().data(), getCurrentProcessID());
    // Flush stdout buffer so python script can be guaranteed to read up to this point.
    fflush(stdout);
    m_isRunning = false;
    
    if (m_stopTimer.isActive())
        m_stopTimer.stop();
    
    if (m_sampleLogSandboxExtension) {
        m_sampleLogSandboxExtension->revoke();
        m_sampleLogSandboxExtension = nullptr;
    }    
}

bool WebMemorySampler::isRunning() const
{
    return m_isRunning;
}
    
void WebMemorySampler::initializeTempLogFile()
{
    m_sampleLogFilePath = FileSystem::openTemporaryFile(processName(), m_sampleLogFile);
    writeHeaders();
}

void WebMemorySampler::initializeSandboxedLogFile(SandboxExtension::Handle&& sampleLogSandboxHandle, const String& sampleLogFilePath)
{
    m_sampleLogSandboxExtension = SandboxExtension::create(WTFMove(sampleLogSandboxHandle));
    if (m_sampleLogSandboxExtension)
        m_sampleLogSandboxExtension->consume();
    m_sampleLogFilePath = sampleLogFilePath;
    m_sampleLogFile = FileSystem::openFile(m_sampleLogFilePath, FileSystem::FileOpenMode::Write);
    writeHeaders();
}

void WebMemorySampler::writeHeaders()
{
    String processDetails = String::format("Process: %s Pid: %d\n", processName().utf8().data(), getCurrentProcessID());

    CString utf8String = processDetails.utf8();
    FileSystem::writeToFile(m_sampleLogFile, utf8String.data(), utf8String.length());
}

void WebMemorySampler::sampleTimerFired()
{
    sendMemoryPressureEvent();
    appendCurrentMemoryUsageToFile(m_sampleLogFile);
}

void WebMemorySampler::stopTimerFired()
{
    if (!m_isRunning)
        return;
    printf("%g seconds elapsed. Stopping memory sampler...\n", m_runningTime);
    stop();
}

void WebMemorySampler::appendCurrentMemoryUsageToFile(FileSystem::PlatformFileHandle&)
{
    // Collect statistics from allocators and get RSIZE metric
    StringBuilder statString;
    WebMemoryStatistics memoryStats = sampleWebKit();

    if (!memoryStats.values.isEmpty()) {
        statString.append(separator);
        for (size_t i = 0; i < memoryStats.values.size(); ++i) {
            statString.append('\n');
            statString.append(separator);
            statString.append(memoryStats.keys[i]);
            appendSpaces(statString, 35 - memoryStats.keys[i].length());
            statString.appendNumber(memoryStats.values[i]);
        }
    }
    statString.append('\n');

    CString utf8String = statString.toString().utf8();
    FileSystem::writeToFile(m_sampleLogFile, utf8String.data(), utf8String.length());
}

}

#endif
