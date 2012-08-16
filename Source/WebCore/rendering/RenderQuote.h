/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef RenderQuote_h
#define RenderQuote_h

#include "Document.h"
#include "QuotesData.h"
#include "RenderStyle.h"
#include "RenderStyleConstants.h"
#include "RenderText.h"

namespace WebCore {

class RenderQuote : public RenderText {
public:
    RenderQuote(Document*, const QuoteType);
    virtual ~RenderQuote();
    void attachQuote();
    void detachQuote();

private:
    virtual void willBeDestroyed() OVERRIDE;
    virtual const char* renderName() const OVERRIDE { return "RenderQuote"; };
    virtual bool isQuote() const OVERRIDE { return true; };
    virtual PassRefPtr<StringImpl> originalText() const OVERRIDE;
    virtual void computePreferredLogicalWidths(float leadWidth) OVERRIDE;

    // We don't override insertedIntoTree to call attachQuote() as it would be attached
    // too early and get the wrong depth since generated content is inserted into anonymous
    // renderers before going into the main render tree. Once we can ensure that insertIntoTree,
    // is called on an attached tree, we should override it here.

    const QuotesData* quotesData() const;
    void updateDepth();
    bool isAttached() { return m_attached; }

    QuoteType m_type;
    int m_depth;
    RenderQuote* m_next;
    RenderQuote* m_previous;
    bool m_attached;
};

inline RenderQuote* toRenderQuote(RenderObject* object)
{
    ASSERT(!object || object->isQuote());
    return static_cast<RenderQuote*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderQuote(const RenderQuote*);

} // namespace WebCore

#endif // RenderQuote_h
