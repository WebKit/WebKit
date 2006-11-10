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

#include "Editor.h"
#include "FormData.h"
#include "FrameTree.h"
#include "KURL.h"
#include "SelectionController.h"
#include "StringHash.h"

namespace WebCore {

    class IconLoader;
    class TextResourceDecoder;
    class UserStyleSheetLoader;

    class FramePrivate {
    public:
        FramePrivate(Page*, Frame* parent, Frame* thisFrame, Element* ownerElement, PassRefPtr<EditorClient>);
        ~FramePrivate();

        Page* m_page;
        FrameTree m_treeNode;
        RefPtr<DOMWindow> m_domWindow;

        Element* m_ownerElement;
        RefPtr<FrameView> m_view;
        RefPtr<Document> m_doc;

        KJSProxy* m_jscript;
        bool m_bJScriptEnabled : 1;
        bool m_bJavaEnabled : 1;
        bool m_bPluginsEnabled : 1;

        Settings* m_settings;

        String m_kjsStatusBarText;
        String m_kjsDefaultStatusBarText;

        int m_zoomFactor;

        bool m_bMousePressed;
        RefPtr<Node> m_mousePressNode; // node under the mouse when the mouse was pressed (set in the mouse handler)

        TextGranularity m_selectionGranularity;
        bool m_beganSelectingText;

        SelectionController m_selectionController;
        Selection m_mark;
        Timer<Frame> m_caretBlinkTimer;
        Editor m_editor;

        bool m_caretVisible : 1;
        bool m_caretPaint : 1;
        bool m_isActive : 1;

        int m_xPosForVerticalArrowNavigation;
        RefPtr<CSSMutableStyleDeclaration> m_typingStyle;

        IntPoint m_dragStartPos;

        Timer<Frame> m_lifeSupportTimer;

        FrameLoader* m_loader;
        UserStyleSheetLoader* m_userStyleSheetLoader;
        
        Timer<Frame> m_autoscrollTimer;
        RenderObject* m_autoscrollRenderer;
        bool m_mouseDownMayStartAutoscroll;
        bool m_mouseDownMayStartDrag;
        
        RefPtr<Node> m_elementToDraw;
        PaintRestriction m_paintRestriction;
        
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
