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

#ifndef khtmlpart_p_h
#define khtmlpart_p_h

#include "EditCommand.h"
#include "Frame.h"
#include "FrameTree.h"
#include "SelectionController.h"
#include "Timer.h"
#include "css_valueimpl.h"
#include "kjs_proxy.h"
#include <kio/global.h>
#include <kxmlcore/Vector.h>

namespace WebCore {

    class Decoder;
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
        FramePrivate(Page* page, Frame* parent, Frame* thisFrame, RenderPart* ownerRenderer)
            : m_page(page)
            , m_treeNode(thisFrame, parent)
            , m_ownerRenderer(ownerRenderer)
            , m_extension(0)
            , m_jscript(0)
            , m_runningScripts(0)
            , m_bJScriptEnabled(true)
            , m_bJavaEnabled(true)
            , m_bPluginsEnabled(true)
            , m_metaRefreshEnabled(true)
            , m_restored(false)
            , m_frameNameId(1)
            , m_settings(0)
            , m_job(0)
            , m_bComplete(true)
            , m_bLoadingMainResource(false)
            , m_bLoadEventEmitted(true)
            , m_bUnloadEventEmitted(true)
            , m_haveEncoding(false)
            , m_bHTTPRefresh(false)
            , m_redirectLockHistory(false)
            , m_redirectUserGesture(false)
            , m_cachePolicy(KIO::CC_Verify)
            , m_redirectionTimer(thisFrame, &Frame::redirectionTimerFired)
            , m_scheduledRedirection(noRedirectionScheduled)
            , m_delayRedirect(0)
            , m_zoomFactor(100)
            , m_submitForm(0)
            , m_bMousePressed(false)
            , m_caretBlinkTimer(thisFrame, &Frame::caretBlinkTimerFired)
            , m_caretVisible(false)
            , m_caretBlinks(true)
            , m_caretPaint(true)
            , m_bDnd(true)
            , m_bFirstData(true)
            , m_bClearing(false)
            , m_bCleared(true)
            , m_bSecurityInQuestion(false)
            , m_focusNodeRestored(false)
            , m_isFocused(false)
            , m_focusNodeNumber(-1)
            , m_opener(0)
            , m_openedByJS(false)
            , m_newJSInterpreterExists(false)
            , m_bPendingChildRedirection(false)
            , m_executingJavaScriptFormAction(false)
            , m_cancelWithLoadInProgress(false)
            , m_lifeSupportTimer(thisFrame, &Frame::lifeSupportTimerFired)
            , m_userStyleSheetLoader(0)
        {
            // inherit settings from parent
            if (parent && parent->isFrame()) {
                Frame *frame = static_cast<Frame*>(parent);
                if (frame->d)
                    m_zoomFactor = frame->d->m_zoomFactor;
            }
        }

        ~FramePrivate()
        {
            delete m_extension;
            delete m_jscript;
        }

        Page* m_page;
        FrameTree m_treeNode;

        Vector<RefPtr<Plugin> > m_plugins;

        QGuardedPtr<RenderPart> m_ownerRenderer;
        RefPtr<FrameView> m_view;
        BrowserExtension* m_extension;
        RefPtr<DocumentImpl> m_doc;
        RefPtr<Decoder> m_decoder;
        QString m_encoding;
        QString scheduledScript;
        RefPtr<NodeImpl> scheduledScriptNode;

        KJSProxyImpl* m_jscript;
        int m_runningScripts;
        bool m_bJScriptEnabled : 1;
        bool m_bJavaEnabled : 1;
        bool m_bPluginsEnabled : 1;
        bool m_metaRefreshEnabled : 1;
        bool m_restored : 1;
        int m_frameNameId;

        KHTMLSettings* m_settings;

        TransferJob* m_job;

        String m_kjsStatusBarText;
        String m_kjsDefaultStatusBarText;
        QString m_lastModified;

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

        KIO::CacheControl m_cachePolicy;
        Timer<Frame> m_redirectionTimer;

        RedirectionScheduled m_scheduledRedirection;
        double m_delayRedirect;
        QString m_redirectURL;
        QString m_redirectReferrer;
        int m_scheduledHistoryNavigationSteps;

        int m_zoomFactor;

        QString m_strSelectedURL;
        QString m_strSelectedURLTarget;
        QString m_referrer;

        struct SubmitForm {
            const char* submitAction;
            QString submitUrl;
            FormData submitFormData;
            QString target;
            QString submitContentType;
            QString submitBoundary;
        };
        SubmitForm* m_submitForm;

        bool m_bMousePressed;
        RefPtr<NodeImpl> m_mousePressNode; // node under the mouse when the mouse was pressed (set in the mouse handler)

        ETextGranularity m_selectionGranularity;
        bool m_beganSelectingText;

        SelectionController m_selection;
        SelectionController m_dragCaret;
        Selection m_mark;
        Timer<Frame> m_caretBlinkTimer;

        bool m_caretVisible : 1;
        bool m_caretBlinks : 1;
        bool m_caretPaint : 1;
        bool m_bDnd : 1;
        bool m_bFirstData : 1;
        bool m_bClearing : 1;
        bool m_bCleared : 1;
        bool m_bSecurityInQuestion : 1;
        bool m_focusNodeRestored : 1;
        bool m_isFocused : 1;

        EditCommandPtr m_lastEditCommand;
        int m_xPosForVerticalArrowNavigation;
        RefPtr<CSSMutableStyleDeclarationImpl> m_typingStyle;

        int m_focusNodeNumber;

        IntPoint m_dragStartPos;

        QGuardedPtr<Frame> m_opener;
        bool m_openedByJS;
        bool m_newJSInterpreterExists; // set to 1 by setOpenedByJS, for window.open
        bool m_bPendingChildRedirection;
        bool m_executingJavaScriptFormAction;
        bool m_cancelWithLoadInProgress;

        Timer<Frame> m_lifeSupportTimer;

        UserStyleSheetLoader* m_userStyleSheetLoader;
    };

}

#endif
