/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptCachedFrameData.h"

#if PLATFORM(QT)
// FIXME: the right guard should be ENABLE(PAGE_CACHE). Replace with the right guard, once
// https://bugs.webkit.org/show_bug.cgi?id=35061 is fixed.

#include "Frame.h"
#include "V8DOMWindow.h"

namespace WebCore {

ScriptCachedFrameData::ScriptCachedFrameData(Frame* frame)
    : m_domWindow(0)
{
    v8::HandleScope handleScope;
    // The context can only be the context of the main world.
    ASSERT(V8Proxy::mainWorldContext(frame) == V8Proxy::context(frame));
    m_context.set(V8Proxy::mainWorldContext(frame));
    // The context can be 0, e.g. if JS is disabled in the browser.
    if (m_context.get().IsEmpty())
        return;
    m_global.set(m_context.get()->Global());
    m_domWindow = frame->domWindow();
}

DOMWindow* ScriptCachedFrameData::domWindow() const
{
    return m_domWindow;
}

void ScriptCachedFrameData::restore(Frame* frame)
{
    if (m_context.get().IsEmpty())
        return;

    if (!frame || !frame->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;

    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(m_context.get());

    m_context.get()->ReattachGlobal(m_global.get());
    V8Proxy* proxy = V8Proxy::retrieve(frame);
    if (proxy)
        proxy->windowShell()->setContext(m_context.get());
}

void ScriptCachedFrameData::clear()
{
    m_context.clear();
    m_global.clear();
}

} // namespace WebCore

#endif // PLATFORM(QT)
