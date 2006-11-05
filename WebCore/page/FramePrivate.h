/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef FramePrivate_h
#define FramePrivate_h

#include "CSSMutableStyleDeclaration.h"
#include "CachePolicy.h"
#include "CommandByName.h"
#include "DOMWindow.h"
#include "TextResourceDecoder.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EditorClient.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "IconLoader.h"
#include "SelectionController.h"
#include "StringHash.h"
#include "Timer.h"
#include "kjs_proxy.h"
#include <wtf/HashMap.h>

namespace WebCore {

    class TextResourceDecoder;
    class UserStyleSheetLoader;

    enum RedirectionScheduled {
        noRedirectionScheduled,
        redirectionScheduled,
        locationChangeScheduled,
        historyNavigationScheduled,
        locationChangeScheduledDuringLoad
    };
    
    class FramePrivate {
    public:
        FramePrivate(Page* page, Frame* parent, Frame* thisFrame, Element* ownerElement, PassRefPtr<EditorClient> client)
            : m_page(page)
            , m_treeNode(thisFrame, parent)
            , m_ownerElement(ownerElement)
            , m_jscript(0)
            , m_runningScripts(0)
            , m_bJScriptEnabled(true)
            , m_bJavaEnabled(true)
            , m_bPluginsEnabled(true)
            , m_settings(0)
            , m_bComplete(true)
            , m_bLoadingMainResource(false)
            , m_bLoadEventEmitted(true)
            , m_bUnloadEventEmitted(true)
            , m_haveEncoding(false)
            , m_bHTTPRefresh(false)
            , m_redirectLockHistory(false)
            , m_redirectUserGesture(false)
            , m_cachePolicy(CachePolicyVerify)
            , m_redirectionTimer(thisFrame, &Frame::redirectionTimerFired)
            , m_scheduledRedirection(noRedirectionScheduled)
            , m_delayRedirect(0)
            , m_zoomFactor(parent ? parent->d->m_zoomFactor : 100)
            , m_submitForm(0)
            , m_bMousePressed(false)
            , m_beganSelectingText(false)
            , m_selectionController(thisFrame)
            , m_caretBlinkTimer(thisFrame, &Frame::caretBlinkTimerFired)
            , m_editor(thisFrame, client)
            , m_command(thisFrame)
            , m_caretVisible(false)
            , m_caretPaint(true)
            , m_bFirstData(true)
            , m_bCleared(true)
            , m_isActive(false)
            , m_opener(0)
            , m_openedByJS(false)
            , m_bPendingChildRedirection(false)
            , m_executingJavaScriptFormAction(false)
            , m_cancelWithLoadInProgress(false)
            , m_lifeSupportTimer(thisFrame, &Frame::lifeSupportTimerFired)
            , m_loader(new FrameLoader(thisFrame))
            , m_userStyleSheetLoader(0)
            , m_iconLoader(0)
            , m_autoscrollTimer(thisFrame, &Frame::autoscrollTimerFired)
            , m_autoscrollRenderer(0)
            , m_mouseDownMayStartAutoscroll(false)
            , m_mouseDownMayStartDrag(false)
            , m_paintRestriction(PaintRestrictionNone)
            , m_markedTextUsesUnderlines(false)
            , m_highlightTextMatches(false)
            , m_windowHasFocus(false)
            , m_inViewSourceMode(false)
            , frameCount(0)
            , m_prohibitsScrolling(false)
        {
        }

        ~FramePrivate()
        {
            delete m_jscript;
            delete m_loader;
            delete m_iconLoader;
        }

        Page* m_page;
        FrameTree m_treeNode;
        RefPtr<DOMWindow> m_domWindow;

        Vector<RefPtr<Plugin> > m_plugins;

        Element* m_ownerElement;
        RefPtr<FrameView> m_view;
        RefPtr<Document> m_doc;
        RefPtr<TextResourceDecoder> m_decoder;
        String m_encoding;
        String scheduledScript;
        RefPtr<Node> scheduledScriptNode;

        KJSProxy* m_jscript;
        int m_runningScripts;
        bool m_bJScriptEnabled : 1;
        bool m_bJavaEnabled : 1;
        bool m_bPluginsEnabled : 1;

        Settings* m_settings;

        HashMap<String, String> m_metaData;

        String m_kjsStatusBarText;
        String m_kjsDefaultStatusBarText;
        String m_lastModified;

        bool m_bComplete : 1;
        bool m_bLoadingMainResource : 1;
        bool m_bLoadEventEmitted : 1;
        bool m_bUnloadEventEmitted : 1;
        bool m_haveEncoding : 1;
        bool m_bHTTPRefresh : 1;
        bool m_redirectLockHistory : 1;
        bool m_redirectUserGesture : 1;

        KURL m_url;
        KURL m_workingURL;

        String m_responseMIMEType;

        CachePolicy m_cachePolicy;
        Timer<Frame> m_redirectionTimer;

        RedirectionScheduled m_scheduledRedirection;
        double m_delayRedirect;
        DeprecatedString m_redirectURL;
        String m_redirectReferrer;
        int m_scheduledHistoryNavigationSteps;

        int m_zoomFactor;

        String m_referrer;

        struct SubmitForm {
            const char* submitAction;
            String submitUrl;
            FormData submitFormData;
            String target;
            String submitContentType;
            String submitBoundary;
            RefPtr<Event> event;
        };
        SubmitForm* m_submitForm;

        bool m_bMousePressed;
        RefPtr<Node> m_mousePressNode; // node under the mouse when the mouse was pressed (set in the mouse handler)

        TextGranularity m_selectionGranularity;
        bool m_beganSelectingText;

        SelectionController m_selectionController;
        Selection m_mark;
        Timer<Frame> m_caretBlinkTimer;
        Editor m_editor;
        CommandByName m_command;

        bool m_caretVisible : 1;
        bool m_caretPaint : 1;
        bool m_bFirstData : 1;
        bool m_bCleared : 1;
        bool m_isActive : 1;

        int m_xPosForVerticalArrowNavigation;
        RefPtr<CSSMutableStyleDeclaration> m_typingStyle;

        IntPoint m_dragStartPos;

        Frame* m_opener;
        HashSet<Frame*> m_openedFrames;
        bool m_openedByJS;
        bool m_bPendingChildRedirection;
        bool m_executingJavaScriptFormAction;
        bool m_cancelWithLoadInProgress;

        Timer<Frame> m_lifeSupportTimer;

        FrameLoader* m_loader;
        UserStyleSheetLoader* m_userStyleSheetLoader;
        IconLoader* m_iconLoader;
        
        Timer<Frame> m_autoscrollTimer;
        RenderObject* m_autoscrollRenderer;
        bool m_mouseDownMayStartAutoscroll;
        bool m_mouseDownMayStartDrag;
        
        RefPtr<Node> m_elementToDraw;
        PaintRestriction m_paintRestriction;
        
        HashMap<String, String> m_formValuesAboutToBeSubmitted;
        RefPtr<Element> m_formAboutToBeSubmitted;
        KURL m_submittedFormURL;
        
        bool m_markedTextUsesUnderlines;
        Vector<MarkedTextUnderline> m_markedTextUnderlines;
        bool m_highlightTextMatches;
        bool m_windowHasFocus;
        
        bool m_inViewSourceMode;

        unsigned frameCount;

        bool m_prohibitsScrolling;
    };
}

#endif
