/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "FormAssociatedElement.h"

namespace WebCore {

// ValidityState is not a separate object, but rather an interface of FormAssociatedElement that
// is published as part of the DOM. We could implement this as a base class of FormAssociatedElement,
// but that would have a small runtime cost, and no significant benefit. We'd prefer to implement this
// as a typedef of FormAssociatedElement, but that would require changes to bindings generation.
class ValidityState : public FormAssociatedElement {
};

inline ValidityState* FormAssociatedElement::validity()
{
    // Because ValidityState adds nothing to FormAssociatedElement, we rely on it being safe
    // to cast a FormAssociatedElement like this, even though it's not actually a ValidityState.
    return static_cast<ValidityState*>(this);
}

} // namespace WebCore
