/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ScriptProfile.h"

#include "JSDOMBinding.h"
#include <inspector/InspectorValues.h>
#include <profiler/Profile.h>
#include <profiler/ProfileNode.h>

namespace WebCore {

PassRefPtr<ScriptProfile> ScriptProfile::create(PassRefPtr<JSC::Profile> profile)
{
    if (!profile)
        return 0;
    return adoptRef(new ScriptProfile(profile));
}

ScriptProfile::ScriptProfile(PassRefPtr<JSC::Profile> profile)
    : m_profile(profile)
{
}

ScriptProfile::~ScriptProfile()
{
}

String ScriptProfile::title() const
{
    return m_profile->title();
}

unsigned int ScriptProfile::uid() const
{
    return m_profile->uid();
}

ScriptProfileNode* ScriptProfile::head() const
{
    return m_profile->head();
}

double ScriptProfile::idleTime() const
{
    return m_profile->idleTime();
}

#if ENABLE(INSPECTOR)
static PassRefPtr<Inspector::TypeBuilder::Profiler::CPUProfileNodeCall> buildInspectorObjectFor(const JSC::ProfileNode::Call& call)
{
    RefPtr<Inspector::TypeBuilder::Profiler::CPUProfileNodeCall> result = Inspector::TypeBuilder::Profiler::CPUProfileNodeCall::create()
        .setStartTime(call.startTime())
        .setTotalTime(call.totalTime());
    return result.release();
}

static PassRefPtr<Inspector::TypeBuilder::Profiler::CPUProfileNode> buildInspectorObjectFor(const JSC::ProfileNode* node)
{
    RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::CPUProfileNodeCall>> calls = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::CPUProfileNodeCall>::create();
    for (const JSC::ProfileNode::Call& call : node->calls())
        calls->addItem(buildInspectorObjectFor(call));

    RefPtr<Inspector::TypeBuilder::Profiler::CPUProfileNode> result = Inspector::TypeBuilder::Profiler::CPUProfileNode::create()
        .setId(node->id())
        .setCalls(calls.release());

    if (!node->functionName().isEmpty())
        result->setFunctionName(node->functionName());

    if (!node->url().isEmpty()) {
        result->setUrl(node->url());
        result->setLineNumber(node->lineNumber());
        result->setColumnNumber(node->columnNumber());
    }

    if (!node->children().isEmpty()) {
        RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::CPUProfileNode>> children = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::CPUProfileNode>::create();
        for (RefPtr<JSC::ProfileNode> profileNode : node->children())
            children->addItem(buildInspectorObjectFor(profileNode.get()));
        result->setChildren(children);
    }

    return result.release();
}

PassRefPtr<Inspector::TypeBuilder::Profiler::CPUProfile> ScriptProfile::buildInspectorObject() const
{
    RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::CPUProfileNode>> rootNodes = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Profiler::CPUProfileNode>::create();
    for (RefPtr<JSC::ProfileNode> profileNode : m_profile->head()->children())
        rootNodes->addItem(buildInspectorObjectFor(profileNode.get()));

    RefPtr<Inspector::TypeBuilder::Profiler::CPUProfile> result = Inspector::TypeBuilder::Profiler::CPUProfile::create()
        .setRootNodes(rootNodes);

    if (m_profile->idleTime())
        result->setIdleTime(m_profile->idleTime());

    return result.release();
}
#endif

} // namespace WebCore
