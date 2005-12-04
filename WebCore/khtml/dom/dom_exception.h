/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 *
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 1 Specification (Recommendation)
 * http://www.w3.org/TR/REC-DOM-Level-1/
 * Copyright © World Wide Web Consortium , (Massachusetts Institute of
 * Technology , Institut National de Recherche en Informatique et en
 * Automatique , Keio University ). All Rights Reserved.
 *
 */
#ifndef _DOM_DOMException_h_
#define _DOM_DOMException_h_

#include <dom/dom_misc.h>

namespace DOM {

/**
 * DOM operations only raise exceptions in &quot;exceptional&quot;
 * circumstances, i.e., when an operation is impossible to perform
 * (either for logical reasons, because data is lost, or because the
 * implementation has become unstable). In general, DOM methods return
 * specific error values in ordinary processing situation, such as
 * out-of-bound errors when using <code> NodeList </code> .
 *
 *  Implementations may raise other exceptions under other
 * circumstances. For example, implementations may raise an
 * implementation-dependent exception if a <code> null </code>
 * argument is passed.
 *
 *  Some languages and object systems do not support the concept of
 * exceptions. For such systems, error conditions may be indicated
 * using native error reporting mechanisms. For some bindings, for
 * example, methods may return error codes similar to those listed in
 * the corresponding method descriptions.
 *
 */
class DOMException
{
public:

    /**
     * An integer indicating the type of error generated.
     *
     */
    enum ExceptionCode {
        INDEX_SIZE_ERR = 1,
        DOMSTRING_SIZE_ERR = 2,
        HIERARCHY_REQUEST_ERR = 3,
        WRONG_DOCUMENT_ERR = 4,
        INVALID_CHARACTER_ERR = 5,
        NO_DATA_ALLOWED_ERR = 6,
        NO_MODIFICATION_ALLOWED_ERR = 7,
        NOT_FOUND_ERR = 8,
        NOT_SUPPORTED_ERR = 9,
        INUSE_ATTRIBUTE_ERR = 10,
        INVALID_STATE_ERR = 11,
        SYNTAX_ERR = 12,
        INVALID_MODIFICATION_ERR = 13,
        NAMESPACE_ERR = 14,
        INVALID_ACCESS_ERR = 15
    };
};

} //namespace

#endif
