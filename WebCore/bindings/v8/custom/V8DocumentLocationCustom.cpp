/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2006 James G. Speth (speth@end.com)
 *  Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 *  Copyright (C) 2007, 2008, 2009 Google Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "V8Document.h"

#include "DOMWindow.h"
#include "Frame.h"
#include "V8Binding.h"
#include "V8Location.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8Document::locationAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    Document* document = V8Document::toNative(info.Holder());
    if (!document->frame())
        return v8::Null();

    DOMWindow* window = document->frame()->domWindow();
    return toV8(window->location());
}

void V8Document::locationAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    Document* document = V8Document::toNative(info.Holder());
    if (!document->frame())
        return;

    DOMWindow* window = document->frame()->domWindow();
    // setLocation does security checks. // XXXMB- verify!
    V8DOMWindowShell::setLocation(window, toWebCoreString(value));
}

} // namespace WebCore
