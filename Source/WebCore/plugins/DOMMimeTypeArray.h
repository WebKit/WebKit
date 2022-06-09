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

#include "ScriptWrappable.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMMimeType;
class Navigator;

class DOMMimeTypeArray final : public ScriptWrappable, public RefCounted<DOMMimeTypeArray> {
    WTF_MAKE_ISO_ALLOCATED(DOMMimeTypeArray);
public:
    static Ref<DOMMimeTypeArray> create(Navigator&, Vector<Ref<DOMMimeType>>&& = { });
    ~DOMMimeTypeArray();

    unsigned length() const;
    RefPtr<DOMMimeType> item(unsigned index);
    RefPtr<DOMMimeType> namedItem(const AtomString& propertyName);
    Vector<AtomString> supportedPropertyNames();

    Navigator* navigator() { return m_navigator.get(); }

private:
    explicit DOMMimeTypeArray(Navigator&, Vector<Ref<DOMMimeType>>&&);
    
    WeakPtr<Navigator> m_navigator;
    Vector<Ref<DOMMimeType>> m_types;
};

} // namespace WebCore
