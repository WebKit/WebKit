/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef JSNodeFilterCondition_h
#define JSNodeFilterCondition_h

#include "NodeFilterCondition.h"
#include "kjs_dom.h"

namespace WebCore {

    class Node;

    class JSNodeFilterCondition : public NodeFilterCondition {
    public:
        JSNodeFilterCondition(KJS::JSObject* filter);
        virtual short acceptNode(Node*) const;
        virtual void mark();

    protected:
        KJS::JSObject* m_filter;
    };

} // namespace WebCore

#endif // JSNodeFilterCondition_h
