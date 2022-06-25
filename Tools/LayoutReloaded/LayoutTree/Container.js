/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
namespace Layout {
class Container : public Box {
public:
    setFirstChild(Layout::Box&);
    setLastChild(Layout::Box&);
 
    Layout::Box* firstChild();
    Layout::Box* firstInFlowChild();
    Layout::Box* firstInFlowOrFloatChild();
    Layout::Box* lastChild();
    Layout::Box* lastInFlowChild();

    bool hasChild();
    bool hasInFlowChild();
    bool hasInFlowOrFloatChild();

    Vector<Layout::Box&> outOfFlowDescendants();
};
}
*/
Layout.Container = class Container extends Layout.Box {
    constructor(node, id) {
        super(node, id);
        this.m_firstChild = null;
        this.m_lastChild = null;
    }

    setFirstChild(firstChild) {
        this.m_firstChild = firstChild;
    }
    
    setLastChild(lastChild) {
        this.m_lastChild = lastChild;
    }

    firstChild() {
        return this.m_firstChild;
    }

    firstInFlowChild() {
        if (!this.hasChild())
            return null;
        let firstChild = this.firstChild();
        if (firstChild.isInFlow())
            return firstChild;
        return firstChild.nextInFlowSibling();
    }

    firstInFlowOrFloatChild() {
        if (!this.hasChild())
            return null;
        let firstChild = this.firstChild();
        if (firstChild.isInFlow() || firstChild.isFloatingPositioned())
            return firstChild;
        return firstChild.nextInFlowOrFloatSibling();
    }

    lastChild() {
        return this.m_lastChild;
    }

    lastInFlowChild() {
        if (!this.hasChild())
            return null;
        let lastChild = this.lastChild();
        if (lastChild.isInFlow())
            return lastChild;
        return lastChild.previousInFlowSibling();
    }

    hasChild() {
        return !!this.firstChild();
    }

    hasInFlowChild() {
        return !!this.firstInFlowChild();
    }

    hasInFlowOrFloatChild() {
        return !!this.firstInFlowOrFloatChild();
    }

    outOfFlowDescendants() {
        if (!this.isPositioned())
            return new Array();
        let outOfFlowBoxes = new Array();
        let descendants = new Array();
        for (let child = this.firstChild(); child; child = child.nextSibling())
            descendants.push(child);
        while (descendants.length) {
            let descendant = descendants.pop();
            if (descendant.isOutOfFlowPositioned() && descendant.containingBlock() == this)
                outOfFlowBoxes.push(descendant);
            if (!descendant.isContainer())
                continue;
            for (let child = descendant.lastChild(); child; child = child.previousSibling())
                descendants.push(child);
        }
        return outOfFlowBoxes;
    }


}
