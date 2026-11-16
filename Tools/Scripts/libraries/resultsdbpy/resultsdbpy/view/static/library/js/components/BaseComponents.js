// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import {
    DOM, REF, diff, EventStream, uuidv4
}
from '../Ref.js';

function ApplyNewChildren(element, oldArray, newArray, childToKeyFunc, itemProcessor) {
    const newChildren = [];
    const needToRemoveChildren = [];
    diff(oldArray, newArray, (item, index) => {
            const child = element.children[index];
            needToRemoveChildren.push(child);
        },
        (item, newIndex, oldIndex) => {
            if (oldIndex !== -1) {
                const exisitingChild = element.children[oldIndex];
                newChildren.push(exisitingChild);
            } else
                newChildren.push(item);
        }, childToKeyFunc);
    newChildren.forEach((child, index) => {
        if (index < element.children.length) {
            if (child !== element.children[index]) {
                if (child instanceof HTMLElement) {
                    element.children[index].before(child);
                } else
                    DOM.before(element.children[index], typeof itemProcessor === "function" ? itemProcessor(child) : child);
            }
        } else {
            if (child instanceof HTMLElement)
                element.append(child);
            else
                DOM.append(element, typeof itemProcessor === "function" ? itemProcessor(child) : child);
        }
    });
    needToRemoveChildren.forEach((child) => {
        // Clear the ref in the removed child, trigger unmount event
        DOM.remove(child);
    });
}

function ListProvider(exporter, providedInterfaces, ...childrenFunctions) {
    const ref = REF.createRef({
        state: {childrenFunctions: childrenFunctions.map(i => i)},
        onStateUpdate: (element, stateDiff, state) => {
            if (stateDiff.childrenFunctions) {
                ApplyNewChildren(element,
                    state.childrenFunctions, stateDiff.childrenFunctions,
                    (child) => child.toString(),
                    (child) => child(...providedInterfaces));
            }
        }
    });
    if (exporter)
        exporter(childrenFunctions => {
            // Make a copy of it, so that DIFF.array works
            ref.setState({childrenFunctions: childrenFunctions.map(i => i)});
        });
    return `<div ref="${ref}">${childrenFunctions.map(childrenFunction => childrenFunction(...providedInterfaces)).join("")}</div>`;
}

function ListProviderReceiver(contentFunction) {
    const key = uuidv4();
    const res = (...providedInterfaces) => contentFunction(...providedInterfaces);
    // Create unique key for each List Item
    res.toString = () => key;
    return res;
}

// Children compoent will retrun string
function ListComponent(exporter, ...children) {
    const ref = REF.createRef({
        state: {children: children},
        onStateUpdate: (element, stateDiff, state) => {
            if (stateDiff.children)
                ApplyNewChildren(element, state.children, stateDiff.children, a => a);
        },
    });

    if (exporter)
        exporter(children => {
            // Make a copy of it, so that DIFF.array works
            ref.setState({children: children.map(i => i)});
        });
    return `<div ref="${ref}">${children.join("")}</div>`;
}

export {ListProvider, ListProviderReceiver, ListComponent};
