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

#if ENABLE(WEB_AUDIO)
#include "Frame.h"
#include "JSAudioContext.h"
#include "Settings.h"

using namespace JSC;

namespace WebCore {

static Settings* settingsForWindowWebAudio(const JSDOMWindow* window)
{
    ASSERT(window);
    if (Frame* frame = window->impl()->frame())
        return frame->settings();
    return 0;
}

JSValue JSDOMWindow::webkitAudioContext(DOMWindow*, ExecState* exec) const
{
    Settings* settings = settingsForWindowWebAudio(this);
    if (settings && settings->webAudioEnabled())
        return getDOMConstructor<JSAudioContextConstructor>(exec, this);
    return jsUndefined();
}

} // namespace WebCore

#endif
