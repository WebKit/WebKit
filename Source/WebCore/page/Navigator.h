/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#include "DOMWindowProperty.h"
#include "JSDOMPromiseDeferred.h"
#include "NavigatorBase.h"
#include "ScriptWrappable.h"
#include "ShareData.h"
#include "Supplementable.h"

namespace WebCore {

class DOMMimeTypeArray;
class DOMPluginArray;

class Navigator final : public NavigatorBase, public ScriptWrappable, public DOMWindowProperty, public Supplementable<Navigator> {
public:
    static Ref<Navigator> create(ScriptExecutionContext& context, DOMWindow& window) { return adoptRef(*new Navigator(context, window)); }
    virtual ~Navigator();

    String appVersion() const;
    DOMPluginArray& plugins();
    DOMMimeTypeArray& mimeTypes();
    bool cookieEnabled() const;
    bool javaEnabled() const;
    const String& userAgent() const final;
    void userAgentChanged();
    bool onLine() const final;
    void share(ScriptExecutionContext&, ShareData, Ref<DeferredPromise>&&);
    
#if PLATFORM(IOS)
    bool standalone() const;
#endif

    void getStorageUpdates();

private:
    explicit Navigator(ScriptExecutionContext&, DOMWindow&);

    mutable RefPtr<DOMPluginArray> m_plugins;
    mutable RefPtr<DOMMimeTypeArray> m_mimeTypes;
    mutable String m_userAgent;
};

}
