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
#include "NavigatorBase.h"
#include "ScriptWrappable.h"
#include "ShareData.h"
#include "Supplementable.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class Blob;
class DeferredPromise;
class DOMMimeTypeArray;
class DOMPluginArray;
class ShareDataReader;

class Navigator final : public NavigatorBase, public ScriptWrappable, public DOMWindowProperty, public Supplementable<Navigator> {
    WTF_MAKE_ISO_ALLOCATED(Navigator);
public:
    static Ref<Navigator> create(ScriptExecutionContext* context, DOMWindow& window) { return adoptRef(*new Navigator(context, window)); }
    virtual ~Navigator();

    String appVersion() const;
    DOMPluginArray& plugins();
    DOMMimeTypeArray& mimeTypes();
    bool pdfViewerEnabled();
    bool cookieEnabled() const;
    bool javaEnabled() const { return false; }
    const String& userAgent() const final;
    String platform() const final;
    void userAgentChanged();
    bool onLine() const final;
    bool canShare(Document&, const ShareData&);
    void share(Document&, const ShareData&, Ref<DeferredPromise>&&);
    
#if PLATFORM(IOS_FAMILY)
    bool standalone() const;
#endif

    void getStorageUpdates();

#if ENABLE(IOS_TOUCH_EVENTS) && !PLATFORM(MACCATALYST)
    int maxTouchPoints() const { return 5; }
#else
    int maxTouchPoints() const { return 0; }
#endif

    GPU* gpu();

private:
    void showShareData(ExceptionOr<ShareDataWithParsedURL&>, Ref<DeferredPromise>&&);
    explicit Navigator(ScriptExecutionContext*, DOMWindow&);

    void initializePluginAndMimeTypeArrays();

    mutable RefPtr<ShareDataReader> m_loader;
    mutable bool m_hasPendingShare { false };
    mutable bool m_pdfViewerEnabled { false };
    mutable RefPtr<DOMPluginArray> m_plugins;
    mutable RefPtr<DOMMimeTypeArray> m_mimeTypes;
    mutable String m_userAgent;
    mutable String m_platform;
    RefPtr<GPU> m_gpuForWebGPU;
};
}
