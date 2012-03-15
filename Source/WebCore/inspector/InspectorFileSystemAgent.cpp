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

#if ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)

#include "InspectorFileSystemAgent.h"

#include "DOMFileSystem.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"

namespace WebCore {

namespace FileSystemAgentState {
static const char fileSystemAgentEnabled[] = "fileSystemAgentEnabled";
}

// static
PassOwnPtr<InspectorFileSystemAgent> InspectorFileSystemAgent::create(InstrumentingAgents* instrumentingAgents, InspectorState* state)
{
    return adoptPtr(new InspectorFileSystemAgent(instrumentingAgents, state));
}

InspectorFileSystemAgent::~InspectorFileSystemAgent()
{
    m_instrumentingAgents->setInspectorFileSystemAgent(0);
}

void InspectorFileSystemAgent::didOpenFileSystem(PassRefPtr<DOMFileSystem> fileSystem)
{
    // TODO(tzik):
    // Store fileSystem and notify the frontend using fileSystemAdded event.
    // fileSystem should be associated with its name.
}

void InspectorFileSystemAgent::fileSystemInvalidated(PassRefPtr<DOMFileSystem> fileSystem)
{
    // TODO(tzik):
    // Discard fileSystem if we have.
    // If there is no other fileSystem who has same name with fileSystem,
    // notify the frontend using fileSystemInvalidated event.
}

void InspectorFileSystemAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(FileSystemAgentState::fileSystemAgentEnabled, m_enabled);

    // TODO(tzik):
    // Pass all available FileSystem to the frontend, using fileSystemAdded event.
}

void InspectorFileSystemAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(FileSystemAgentState::fileSystemAgentEnabled, m_enabled);
}

void InspectorFileSystemAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(frontend);
    m_frontend = frontend->filesystem();
}

void InspectorFileSystemAgent::clearFrontend()
{
    m_frontend = 0;
    m_enabled = false;
    m_state->setBoolean(FileSystemAgentState::fileSystemAgentEnabled, m_enabled);
}

void InspectorFileSystemAgent::restore()
{
    m_enabled = m_state->getBoolean(FileSystemAgentState::fileSystemAgentEnabled);
}

InspectorFileSystemAgent::InspectorFileSystemAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state)
    : InspectorBaseAgent<InspectorFileSystemAgent>("FileSystem", instrumentingAgents, state),
      m_frontend(0),
      m_enabled(false)
{
    ASSERT(instrumentingAgents);
    ASSERT(state);
    m_instrumentingAgents->setInspectorFileSystemAgent(this);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)
