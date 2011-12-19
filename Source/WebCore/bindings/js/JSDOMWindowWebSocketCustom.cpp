/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "JSDOMWindowCustom.h"

#if ENABLE(WEB_SOCKETS)
#include "Frame.h"
#include "JSWebSocket.h"
#include "Settings.h"

using namespace JSC;

namespace WebCore {

static Settings* settingsForWindowWebSocket(const JSDOMWindow* window)
{
    ASSERT(window);
    if (Frame* frame = window->impl()->frame())
        return frame->settings();
    return 0;
}

JSValue JSDOMWindow::webSocket(DOMWindow*, ExecState* exec) const
{
    if (!settingsForWindowWebSocket(this))
        return jsUndefined();
    return getDOMConstructor<JSWebSocketConstructor>(exec, this);
}

} // namespace WebCore

#endif
