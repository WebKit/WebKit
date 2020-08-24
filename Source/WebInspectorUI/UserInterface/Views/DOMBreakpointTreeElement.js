/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.DOMBreakpointTreeElement = class DOMBreakpointTreeElement extends WI.BreakpointTreeElement
{
    constructor(breakpoint, {classNames, title} = {})
    {
        console.assert(breakpoint instanceof WI.DOMBreakpoint);

        if (!Array.isArray(classNames))
            classNames = [];
        classNames.push("dom", breakpoint.type);

        if (!title)
            title = WI.DOMBreakpointTreeElement.displayNameForType(breakpoint.type);

        super(breakpoint, {classNames, title});
    }

    // Static

    static displayNameForType(type)
    {
        switch (type) {
        case WI.DOMBreakpoint.Type.SubtreeModified:
            return WI.UIString("Subtree Modified", "A submenu item of 'Break On' that breaks (pauses) before child DOM node is modified");
        case WI.DOMBreakpoint.Type.AttributeModified:
            return WI.UIString("Attribute Modified", "A submenu item of 'Break On' that breaks (pauses) before DOM attribute is modified");
        case WI.DOMBreakpoint.Type.NodeRemoved:
            return WI.UIString("Node Removed", "A submenu item of 'Break On' that breaks (pauses) before DOM node is removed");
        default:
            console.error("Unexpected DOM breakpoint type: " + type);
            return null;
        }
    }
};
