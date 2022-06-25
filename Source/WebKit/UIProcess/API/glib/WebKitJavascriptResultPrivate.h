/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <WebCore/SerializedScriptValue.h>
#include "WebKitJavascriptResult.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>

class SharedJavascriptContext {
public:
    static SharedJavascriptContext& singleton()
    {
        static NeverDestroyed<SharedJavascriptContext> context;
        return context;
    }

    SharedJavascriptContext()
        : m_timer(RunLoop::main(), this, &SharedJavascriptContext::releaseContext)
    {
    }

    JSCContext* getOrCreateContext()
    {
        if (!m_context) {
            m_context = adoptGRef(jsc_context_new());
            m_timer.startOneShot(1_s);
        }
        return m_context.get();
    }

private:
    void releaseContext()
    {
        m_context = nullptr;
    }

    GRefPtr<JSCContext> m_context;
    RunLoop::Timer<SharedJavascriptContext> m_timer;
};

WebKitJavascriptResult* webkitJavascriptResultCreate(WebCore::SerializedScriptValue&);
