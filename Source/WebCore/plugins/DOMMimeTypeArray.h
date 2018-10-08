/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include "DOMMimeType.h"
#include "DOMWindowProperty.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class PluginData;

class DOMMimeTypeArray : public ScriptWrappable, public RefCounted<DOMMimeTypeArray>, public DOMWindowProperty {
public:
    static Ref<DOMMimeTypeArray> create(DOMWindow* window) { return adoptRef(*new DOMMimeTypeArray(window)); }
    ~DOMMimeTypeArray();

    unsigned length() const;
    RefPtr<DOMMimeType> item(unsigned index);
    RefPtr<DOMMimeType> namedItem(const AtomicString& propertyName);
    Vector<AtomicString> supportedPropertyNames();

private:
    explicit DOMMimeTypeArray(DOMWindow*);
    PluginData* getPluginData() const;
};

} // namespace WebCore
