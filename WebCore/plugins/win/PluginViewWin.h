/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PluginViewWin_H
#define PluginViewWin_H

#include <winsock2.h>
#include <windows.h>

#include "CString.h"
#include "IntRect.h"
#include "KURL.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
#include "Timer.h"
#include "Widget.h"
#include "npapi.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace KJS {
    namespace Bindings {
        class Instance;
    }
}

namespace WebCore {
    class Element;
    class Frame;
    struct FrameLoadRequest;
    class KeyboardEvent;
    class MouseEvent;
    class KURL;
    class PluginMessageThrottlerWin;
    class PluginPackageWin;
    class PluginRequestWin;
    class PluginStreamWin;
    class ResourceError;
    class ResourceResponse;
    
    enum PluginQuirks {
        PluginQuirkWantsMozillaUserAgent = 1 << 0,
        PluginQuirkDeferFirstSetWindowCall = 1 << 1,
        PluginQuirkThrottleInvalidate = 1 << 2, 
        PluginQuirkRemoveWindowlessVideoParam = 1 << 3,
        PluginQuirkThrottleWMUserPlusOneMessages = 1 << 4,
        PluginQuirkDontUnloadPlugin = 1 << 5,
        PluginQuirkDontCallWndProcForSameMessageRecursively = 1 << 6
    };

    enum PluginStatus {
        PluginStatusCanNotFindPlugin,
        PluginStatusCanNotLoadPlugin,
        PluginStatusLoadedSuccessfully
    };

    class PluginViewWin : public Widget {
    friend static LRESULT CALLBACK PluginViewWndProc(HWND, UINT, WPARAM, LPARAM);

    public:
        PluginViewWin(Frame* parentFrame, const IntSize&, PluginPackageWin* plugin, Element*, const KURL&, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually);
        virtual ~PluginViewWin();

        PluginPackageWin* plugin() const { return m_plugin.get(); }
        NPP instance() const { return m_instance; }

        void setNPWindowRect(const IntRect&);
        static PluginViewWin* currentPluginView();

        KJS::Bindings::Instance* bindingInstance();

        PluginStatus status() const { return m_status; }

        // NPN functions
        NPError getURLNotify(const char* url, const char* target, void* notifyData);
        NPError getURL(const char* url, const char* target);
        NPError postURLNotify(const char* url, const char* target, uint32 len, const char* but, NPBool file, void* notifyData);
        NPError postURL(const char* url, const char* target, uint32 len, const char* but, NPBool file);
        NPError newStream(NPMIMEType type, const char* target, NPStream** stream);
        int32 write(NPStream* stream, int32 len, void* buffer);
        NPError destroyStream(NPStream* stream, NPReason reason);
        const char* userAgent();
        void status(const char* message);
        NPError getValue(NPNVariable variable, void* value);
        NPError setValue(NPPVariable variable, void* value);
        void invalidateRect(NPRect*);
        void invalidateRegion(NPRegion);
        void forceRedraw();

        void disconnectStream(PluginStreamWin*);

        // Widget functions
        virtual void setFrameGeometry(const IntRect&);
        virtual void geometryChanged() const;
        virtual void setFocus();
        virtual void show();
        virtual void hide();
        virtual void paint(GraphicsContext*, const IntRect&);
        virtual IntRect windowClipRect() const;
        virtual void handleEvent(Event*);
        virtual void setParent(ScrollView*);

        LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
        WNDPROC pluginWndProc() const { return m_pluginWndProc; }

        // Used for manual loading
        void didReceiveResponse(const ResourceResponse&);
        void didReceiveData(const char*, int);
        void didFinishLoading();
        void didFail(const ResourceError&);
    private:
        void setParameters(const Vector<String>& paramNames, const Vector<String>& paramValues);
        void init();
        bool start();
        void stop();
        static void setCurrentPluginView(PluginViewWin*);
        NPError load(const FrameLoadRequest&, bool sendNotification, void* notifyData);
        NPError handlePost(const char* url, const char* target, uint32 len, const char* buf, bool file, void* notifyData, bool sendNotification, bool allowHeaders);
        RefPtr<PluginPackageWin> m_plugin;
        Element* m_element;
        Frame* m_parentFrame;
        bool m_isStarted;
        KURL m_url;
        KURL m_baseURL;
        PluginStatus m_status;
        Vector<IntRect> m_invalidRects;

        void performRequest(PluginRequestWin*);
        void scheduleRequest(PluginRequestWin*);
        void requestTimerFired(Timer<PluginViewWin>*);
        void invalidateTimerFired(Timer<PluginViewWin>*);
        Timer<PluginViewWin> m_requestTimer;
        Timer<PluginViewWin> m_invalidateTimer;

        OwnPtr<PluginMessageThrottlerWin> m_messageThrottler;

        void updateWindow() const;
        void determineQuirks(const String& mimeType);
        void paintMissingPluginIcon(GraphicsContext*, const IntRect&);

        void handleKeyboardEvent(KeyboardEvent*);
        void handleMouseEvent(MouseEvent*);

        int m_mode;
        int m_paramCount;
        char** m_paramNames;
        char** m_paramValues;

        CString m_mimeType;
        CString m_userAgent;
        
        NPP m_instance;
        NPP_t m_instanceStruct;
        NPWindow m_npWindow;

        HashSet<RefPtr<PluginStreamWin> > m_streams;
        Vector<PluginRequestWin*> m_requests;

        int m_quirks;
        bool m_isWindowed;
        bool m_isTransparent;
        bool m_isVisible;
        bool m_haveInitialized;

        WNDPROC m_pluginWndProc;
        HWND m_window; // for windowed plug-ins
        mutable IntRect m_clipRect; // The clip rect to apply to a windowed plug-in
        mutable IntRect m_windowRect; // Our window rect.

        unsigned m_lastMessage;
        bool m_isCallingPluginWndProc;

        bool m_loadManually;
        RefPtr<PluginStreamWin> m_manualStream;

        static PluginViewWin* s_currentPluginView;
    };

} // namespace WebCore

#endif 
