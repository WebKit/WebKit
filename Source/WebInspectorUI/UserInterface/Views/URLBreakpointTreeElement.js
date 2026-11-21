/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.URLBreakpointTreeElement = class URLBreakpointTreeElement extends WI.BreakpointTreeElement
{
    constructor(breakpoint, {classNames, title} = {})
    {
        console.assert(breakpoint instanceof WI.URLBreakpoint);

        if (!Array.isArray(classNames))
            classNames = [];
        classNames.push("url");

        if (!title)
            title = breakpoint.displayName;

        super(breakpoint, {classNames, title});
    }

    // Public

    populateContextMenu(contextMenu, event)
    {
        WI.URLBreakpointPopover.appendContextMenuItems(contextMenu, this.breakpoint, this.status, this);

        super.populateContextMenu(contextMenu, event);
    }

    // Popover delegate

    willDismissPopover(popover)
    {
        console.assert(popover instanceof WI.URLBreakpointPopover, popover);

        let breakpoint = popover.breakpoint;
        if (!breakpoint || breakpoint === this.breakpoint)
            return;

        let matches = (existing) => existing && existing !== this.breakpoint && existing.equals(breakpoint);
        if (matches(WI.domDebuggerManager.allRequestsBreakpoint) || WI.domDebuggerManager.urlBreakpoints.some(matches)) {
            InspectorFrontendHost.beep();
            return;
        }

        let wasSelected = this.selected;
        let treeOutline = this.treeOutline;

        this.breakpoint.remove();
        WI.domDebuggerManager.addURLBreakpoint(breakpoint);

        if (wasSelected) {
            const omitFocus = true;
            const selectedByUser = false;
            const suppressNotification = true;
            treeOutline?.findTreeElement(breakpoint)?.select(omitFocus, selectedByUser, suppressNotification);
        }
    }

    // Private

    _handleStatusImageElementDoubleClicked(event)
    {
        WI.URLBreakpointPopover.show(this.breakpoint, this.status, this);
    }
};
