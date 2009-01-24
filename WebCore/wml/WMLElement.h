/*
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef WMLElement_h
#define WMLElement_h

#if ENABLE(WML)
#include "StyledElement.h"
#include "WMLErrorHandling.h"

namespace WebCore {

class WMLElement : public StyledElement {
public:
    WMLElement(const QualifiedName& tagName, Document*);

    virtual bool isWMLElement() const { return true; }
    virtual bool isWMLTaskElement() const { return false; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

protected:
    // Helper function for derived classes
    String parseValueSubstitutingVariableReferences(const AtomicString&, WMLErrorCode defaultErrorCode = WMLErrorInvalidVariableReference);
    String parseValueForbiddingVariableReferences(const AtomicString&);
};

}

#endif
#endif
