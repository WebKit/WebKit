/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#pragma once

#include <WebCore/Document.h>
#include <WebCore/Editor.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/LocalFrame.h>

namespace WebCore {

inline Document* LocalFrame::document() const
{
    return m_doc.get();
}

inline RefPtr<Document> LocalFrame::protectedDocument() const
{
    return document();
}

inline Editor& LocalFrame::editor()
{
    return protectedDocument()->editor();
}

inline const Editor& LocalFrame::editor() const
{
    return protectedDocument()->editor();
}

inline Ref<Editor> LocalFrame::protectedEditor()
{
    return editor();
}

inline Ref<const Editor> LocalFrame::protectedEditor() const
{
    return editor();
}

inline FrameSelection& LocalFrame::selection()
{
    return document()->selection();
}

inline const FrameSelection& LocalFrame::selection() const
{
    return document()->selection();
}

inline CheckedRef<FrameSelection> LocalFrame::checkedSelection() const
{
    return document()->selection();
}

} // namespace WebCore
