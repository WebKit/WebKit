/* Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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

#ifndef FramePrivate_h
#define FramePrivate_h

#include "AnimationController.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "Range.h"
#include "SelectionController.h"
#include "StringHash.h"
#include "kjs_proxy.h"

namespace KJS {
    class Interpreter;
        
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

#if PLATFORM(MAC)
#ifdef __OBJC__
@class WebScriptObject;
#else
class WebScriptObject;
#endif
#endif

#if PLATFORM(WIN)
#include "FrameWin.h"
#endif

namespace WebCore {

#if FRAME_LOADS_USER_STYLESHEET
    class UserStyleSheetLoader;
#endif

    typedef HashMap<void*, RefPtr<KJS::Bindings::RootObject> > RootObjectMap;
    
    class FramePrivate {
    public:
        FramePrivate(Page*, Frame* parent, Frame* thisFrame, HTMLFrameOwnerElement*, FrameLoaderClient*);
        ~FramePrivate();

        Page* m_page;
        FrameTree m_treeNode;
        FrameLoader m_loader;
        RefPtr<DOMWindow> m_domWindow;
        HashSet<DOMWindow*> m_liveFormerWindows;

        HTMLFrameOwnerElement* m_ownerElement;
        RefPtr<FrameView> m_view;
        RefPtr<Document> m_doc;

        KJSProxy m_jscript;

        String m_kjsStatusBarText;
        String m_kjsDefaultStatusBarText;

        float m_zoomFactor;
        bool m_zoomFactorIsTextOnly;

        TextGranularity m_selectionGranularity;

        SelectionController m_selectionController;
        Selection m_mark;
        Timer<Frame> m_caretBlinkTimer;
        Editor m_editor;
        EventHandler m_eventHandler;
        AnimationController m_animationController;

        bool m_caretVisible : 1;
        bool m_caretPaint : 1;
        bool m_isPainting : 1;

        RefPtr<CSSMutableStyleDeclaration> m_typingStyle;

        Timer<Frame> m_lifeSupportTimer;

        RefPtr<Node> m_elementToDraw;
        PaintRestriction m_paintRestriction;
        
        bool m_highlightTextMatches;
        
        bool m_inViewSourceMode;

        unsigned frameCount;

        bool m_prohibitsScrolling;

        bool m_needsReapplyStyles;

        // The root object used for objects bound outside the context of a plugin.
        RefPtr<KJS::Bindings::RootObject> m_bindingRootObject; 
        RootObjectMap m_rootObjects;
#if ENABLE(NETSCAPE_PLUGIN_API)
        NPObject* m_windowScriptNPObject;
#endif
#if FRAME_LOADS_USER_STYLESHEET
        UserStyleSheetLoader* m_userStyleSheetLoader;
#endif
#if PLATFORM(MAC)
        RetainPtr<WebScriptObject> m_windowScriptObject;
#endif
    };
}

#endif
