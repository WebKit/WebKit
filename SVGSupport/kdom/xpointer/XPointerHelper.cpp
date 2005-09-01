/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich     <frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <qglobal.h>

#include <kdebug.h>

#include "kdom.h"
#include "DOMString.h"

#include "DOMExceptionImpl.h"

#include "XPointerHelper.h"
#include "kdomxpointer.h"

#include "XPointerExceptionImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

DOMString XPointerHelper::EncodeSchemeData(const DOMString &string)
{
    QString result;
    const unsigned int length = string.length();

    for(unsigned int i = 0; i < length; i++)
    {
        QChar at = string[i];

        if(at == '^' || at == '(' || at == ')')
            result += '^';

        result += at;
    }

    return DOMString(result);
}

DOMString XPointerHelper::DecodeSchemeData(const DOMString &string)
{
    QString result;
    const unsigned int length = string.length();

    for(unsigned int i = 0; i < length; i++)
    {
        QChar at = string[i];

        if(at == '^')
        {
            QChar of = string[i + 1];

            if(of != '^' && of != '(' && of != ')')
                throw new XPointerExceptionImpl(INVALID_ENCODING);

            result += of;
            i++;
        }
        else
            result += at;
    }

    return DOMString(result);
}

/**
 * Creates an xpointer locating @p node. The XPointer is in the element scheme.
 *
 * @throws KDOM::NOT_FOUND_ERR when the node is null.
 */

/* Two overloads are perhaps needed in addition:
 * DOMString createXPointer(const Range &range);
 * DOMString createXPointer(const XPointerResult &result); // XPointerResult is list of Ranges
 */
#if 0
DOMString XPointerHelper::createXPointer(const Element& node)
{
    if(node == Element::null)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);
       
    if(node.nodeType() == TEXT_NODE) /* Not supported. */
        return DOMString();

    QString childSequence;
    unsigned long count = 0;

    /* Do the moon walk. */
    Node child = node;
    do
    {
        do
        {
            if(child.nodeType() == ELEMENT_NODE) /* Includes ourselves */
                count++;

            child = child.previousSibling();
        }
        while(child != Node::null);

        Q_ASSERT(count != 0);

        childSequence.prepend('/' + QString::number(count));
        count = 0; /* Reset for the next level. */
        child = child.parentNode();
    }
    while(child.nodeType() != DOCUMENT_NODE);

    Q_ASSERT(!childSequence.contains("//"));
    Q_ASSERT(!childSequence.endsWith("/"));
    Q_ASSERT(childSequence.startsWith("/"));

    return DOMString(QString("element(") + childSequence + QString(")"));
}
#endif

// vim:ts=4:noet
