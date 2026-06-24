/*
 * Copyright (C) 2026 Igalia S.L.
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

#include "PopupMenu.h"
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Event;
class EventListener;
class HTMLElement;
class HTMLSelectElement;
class IntRect;
class LocalFrameView;
class Node;
class WeakPtrImplWithEventTargetData;

class FallbackPopupMenu : public PopupMenu, public CanMakeWeakPtr<FallbackPopupMenu> {
    WTF_MAKE_TZONE_ALLOCATED(FallbackPopupMenu);
    WTF_MAKE_NONCOPYABLE(FallbackPopupMenu);
public:
    static Ref<FallbackPopupMenu> create(HTMLSelectElement& element) { return adoptRef(*new FallbackPopupMenu(element)); }
    ~FallbackPopupMenu();

    void show(const IntRect&, LocalFrameView&, int selectedIndex) override;
    void hide() override;
    void updateFromElement() override;
    void disconnectClient() override;

    void itemClicked(int listIndex);
    void maybeClose(Node&, Event&);

private:
    explicit FallbackPopupMenu(HTMLSelectElement&);

    void buildPopupTree(const IntRect& elementBounds, LocalFrameView&);
    void deletePopupTree();

    void updateFromElementTimerFired();

    WeakPtr<HTMLSelectElement, WeakPtrImplWithEventTargetData> m_element;
    RefPtr<HTMLElement> m_container;
    RefPtr<EventListener> m_dismissListener;
    RunLoop::Timer m_updateTimer;
};

} // namespace WebCore

