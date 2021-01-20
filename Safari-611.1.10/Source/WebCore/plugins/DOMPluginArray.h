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

#include "Navigator.h"
#include "ScriptWrappable.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMPlugin;
class Navigator;

class DOMPluginArray final : public ScriptWrappable, public RefCounted<DOMPluginArray> {
    WTF_MAKE_ISO_ALLOCATED(DOMPluginArray);
public:
    static Ref<DOMPluginArray> create(Navigator&, Vector<Ref<DOMPlugin>>&& = { }, Vector<Ref<DOMPlugin>>&& = { });
    ~DOMPluginArray();

    unsigned length() const;
    RefPtr<DOMPlugin> item(unsigned index);
    RefPtr<DOMPlugin> namedItem(const AtomString& propertyName);
    Vector<AtomString> supportedPropertyNames();

    void refresh(bool reloadPages);

    Navigator* navigator() { return m_navigator.get(); }
    
private:
    explicit DOMPluginArray(Navigator&, Vector<Ref<DOMPlugin>>&&, Vector<Ref<DOMPlugin>>&&);


    WeakPtr<Navigator> m_navigator;
    Vector<Ref<DOMPlugin>> m_publiclyVisiblePlugins;
    Vector<Ref<DOMPlugin>> m_additionalWebVisibilePlugins;
};

} // namespace WebCore
