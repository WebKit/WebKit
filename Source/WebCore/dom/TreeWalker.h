/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#ifndef TreeWalker_h
#define TreeWalker_h

#include "NodeFilter.h"
#include "ScriptWrappable.h"
#include "Traversal.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    typedef int ExceptionCode;

    class TreeWalker : public ScriptWrappable, public RefCounted<TreeWalker>, public NodeIteratorBase {
    public:
        static PassRefPtr<TreeWalker> create(PassRefPtr<Node> rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> filter, bool expandEntityReferences)
        {
            return adoptRef(new TreeWalker(rootNode, whatToShow, filter, expandEntityReferences));
        }                            

        Node* currentNode() const { return m_current.get(); }
        void setCurrentNode(PassRefPtr<Node>, ExceptionCode&);

        Node* parentNode(JSC::ExecState*);
        Node* firstChild(JSC::ExecState*);
        Node* lastChild(JSC::ExecState*);
        Node* previousSibling(JSC::ExecState*);
        Node* nextSibling(JSC::ExecState*);
        Node* previousNode(JSC::ExecState*);
        Node* nextNode(JSC::ExecState*);

        // Do not call these functions. They are just scaffolding to support the Objective-C bindings.
        // They operate in the main thread normal world, and they swallow JS exceptions.
        Node* parentNode() { return parentNode(execStateFromNode(mainThreadNormalWorld(), m_current.get())); }
        Node* firstChild() { return firstChild(execStateFromNode(mainThreadNormalWorld(), m_current.get())); }
        Node* lastChild() { return lastChild(execStateFromNode(mainThreadNormalWorld(), m_current.get())); }
        Node* previousSibling() { return previousSibling(execStateFromNode(mainThreadNormalWorld(), m_current.get())); }
        Node* nextSibling() { return nextSibling(execStateFromNode(mainThreadNormalWorld(), m_current.get())); }
        Node* previousNode() { return previousNode(execStateFromNode(mainThreadNormalWorld(), m_current.get())); }
        Node* nextNode() { return nextNode(execStateFromNode(mainThreadNormalWorld(), m_current.get())); }

    private:
        TreeWalker(PassRefPtr<Node>, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences);
        
        Node* setCurrent(PassRefPtr<Node>);

        RefPtr<Node> m_current;
    };

} // namespace WebCore

#endif // TreeWalker_h
