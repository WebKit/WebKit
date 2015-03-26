/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.DetailsSidebarPanel = class DetailsSidebarPanel extends WebInspector.SidebarPanel
{
    constructor(identifier, displayName, singularDisplayName, image, keyboardShortcutKey, element)
    {
        var keyboardShortcut = null;
        if (keyboardShortcutKey)
            keyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Control | WebInspector.KeyboardShortcut.Modifier.Shift, keyboardShortcutKey);

        if (keyboardShortcut) {
            var showToolTip = WebInspector.UIString("Show the %s details sidebar (%s)").format(singularDisplayName, keyboardShortcut.displayName);
            var hideToolTip = WebInspector.UIString("Hide the %s details sidebar (%s)").format(singularDisplayName, keyboardShortcut.displayName);
        } else {
            var showToolTip = WebInspector.UIString("Show the %s details sidebar").format(singularDisplayName);
            var hideToolTip = WebInspector.UIString("Hide the %s details sidebar").format(singularDisplayName);
        }

        super(identifier, displayName, showToolTip, hideToolTip, image, element);

        this._keyboardShortcut = keyboardShortcut;
        if (this._keyboardShortcut)
            this._keyboardShortcut.callback = this.toggle.bind(this);

        this.element.classList.add("details");
    }

    // Public

    inspect(objects)
    {
        // Implemented by subclasses.
        return false;
    }

    shown()
    {
        if (this._needsRefresh) {
            delete this._needsRefresh;
            this.refresh();
        }
    }

    needsRefresh()
    {
        if (!this.selected) {
            this._needsRefresh = true;
            return;
        }

        this.refresh();
    }

    refresh()
    {
        // Implemented by subclasses.
    }
};
