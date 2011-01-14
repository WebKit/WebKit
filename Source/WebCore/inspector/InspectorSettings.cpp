/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "config.h"
#include "InspectorSettings.h"

#if ENABLE(INSPECTOR)

#include "InspectorClient.h"

namespace WebCore {

const char* InspectorSettings::MonitoringXHREnabled = "xhrMonitor";
const char* InspectorSettings::ProfilerAlwaysEnabled = "profilerEnabled";
const char* InspectorSettings::DebuggerAlwaysEnabled = "debuggerEnabled";
const char* InspectorSettings::InspectorStartsAttached = "inspectorStartsAttached";
const char* InspectorSettings::InspectorAttachedHeight = "inspectorAttachedHeight";

InspectorSettings::InspectorSettings(InspectorClient* client)
    : m_client(client)
{
    registerBoolean(MonitoringXHREnabled, false);
    registerBoolean(ProfilerAlwaysEnabled, false);
    registerBoolean(DebuggerAlwaysEnabled, false);
    registerBoolean(InspectorStartsAttached, true);
    registerLong(InspectorAttachedHeight, 300);
}

bool InspectorSettings::getBoolean(const String& name)
{
    String value;
    m_client->populateSetting(name, &value);
    if (value.isEmpty())
        value = m_defaultValues.get(name);
    return value == "true";
}

void InspectorSettings::setBoolean(const String& name, bool value)
{
    m_client->storeSetting(name, value ? "true" : "false");
}

long InspectorSettings::getLong(const String& name)
{
    String value;
    m_client->populateSetting(name, &value);
    if (value.isEmpty())
        value = m_defaultValues.get(name);
    return value.toInt();
}

void InspectorSettings::setLong(const String& name, long value)
{
    m_client->storeSetting(name, String::number(value));
}

void InspectorSettings::registerBoolean(const String& name, bool defaultValue)
{
    m_defaultValues.set(name, defaultValue ? "true" : "false");
}

void InspectorSettings::registerLong(const String& name, long defaultValue)
{
    m_defaultValues.set(name, String::number(defaultValue));
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
