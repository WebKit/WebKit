/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "LabelableElement.h"

namespace WebCore {

class HTMLLabelElement final : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLLabelElement);
public:
    static Ref<HTMLLabelElement> create(const QualifiedName&, Document&);

    WEBCORE_EXPORT RefPtr<LabelableElement> control() const;
    WEBCORE_EXPORT HTMLFormElement* form() const final;

    bool willRespondToMouseClickEvents() final;

private:
    HTMLLabelElement(const QualifiedName&, Document&);

    bool isEventTargetedAtInteractiveDescendants(Event&) const;

    void accessKeyAction(bool sendMouseEvents) final;

    // Overridden to update the hover/active state of the corresponding control.
    void setActive(bool = true, bool pause = false) final;
    void setHovered(bool = true) final;

    // Overridden to either click() or focus() the corresponding control.
    void defaultEventHandler(Event&) final;

    void focus(bool restorePreviousSelection, FocusDirection) final;

    bool isInteractiveContent() const final { return true; }
};

} //namespace
