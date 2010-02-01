/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "V8BarInfo.h"

#include "V8DOMWindow.h"
#include "V8DOMWrapper.h"

namespace WebCore {

v8::Handle<v8::Value> toV8(BarInfo* impl)
{
    if (!impl)
        return v8::Null();
    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(impl);
    if (wrapper.IsEmpty()) {
        wrapper = V8BarInfo::wrap(impl);
        if (!wrapper.IsEmpty()) {
            Frame* frame = impl->frame();
            switch (impl->type()) {
            case BarInfo::Locationbar:
                V8DOMWrapper::setHiddenWindowReference(frame, V8DOMWindow::locationbarIndex, wrapper);
                break;
            case BarInfo::Menubar:
                V8DOMWrapper::setHiddenWindowReference(frame, V8DOMWindow::menubarIndex, wrapper);
                break;
            case BarInfo::Personalbar:
                V8DOMWrapper::setHiddenWindowReference(frame, V8DOMWindow::personalbarIndex, wrapper);
                break;
            case BarInfo::Scrollbars:
                V8DOMWrapper::setHiddenWindowReference(frame, V8DOMWindow::scrollbarsIndex, wrapper);
                break;
            case BarInfo::Statusbar:
                V8DOMWrapper::setHiddenWindowReference(frame, V8DOMWindow::statusbarIndex, wrapper);
                break;
            case BarInfo::Toolbar:
                V8DOMWrapper::setHiddenWindowReference(frame, V8DOMWindow::toolbarIndex, wrapper);
                break;
            }
        }
    }
    return wrapper;
}

} // namespace WebCore
