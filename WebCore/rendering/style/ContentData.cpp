/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ContentData.h"

#include "CounterContent.h"
#include "StringImpl.h"
#include "StyleImage.h"

namespace WebCore {

void ContentData::clear()
{
    deleteContent();

    ContentData* n = m_next;
    m_next = 0;

    // Reverse the list so we can delete without recursing.
    ContentData* last = 0;
    ContentData* c;
    while ((c = n)) {
        n = c->m_next;
        c->m_next = last;
        last = c;
    }
    for (c = last; c; c = n) {
        n = c->m_next;
        c->m_next = 0;
        delete c;
    }
}

bool ContentData::dataEquivalent(const ContentData& other) const
{
    if (type() != other.type())
        return false;

    switch (type()) {
        case CONTENT_NONE:
            return true;
            break;
        case CONTENT_TEXT:
            return equal(text(), other.text());
            break;
        case CONTENT_OBJECT:
            return StyleImage::imagesEquivalent(image(), other.image());
            break;
        case CONTENT_COUNTER:
            return *counter() == *other.counter();
            break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void ContentData::deleteContent()
{
    switch (m_type) {
        case CONTENT_NONE:
            break;
        case CONTENT_OBJECT:
            m_content.m_image->deref();
            break;
        case CONTENT_TEXT:
            m_content.m_text->deref();
            break;
        case CONTENT_COUNTER:
            delete m_content.m_counter;
            break;
    }

    m_type = CONTENT_NONE;
}

} // namespace WebCore
