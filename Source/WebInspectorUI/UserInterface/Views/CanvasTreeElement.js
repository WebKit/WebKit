/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.CanvasTreeElement = class CanvasTreeElement extends WebInspector.FolderizedTreeElement
{
    constructor(canvas)
    {
        super("canvas-icon", canvas.displayName, null, canvas, false);

        console.assert(canvas instanceof WebInspector.Canvas);

        this.small = true;
        this.folderSettingsKey = canvas.parentFrame.url.hash + canvas.displayName.hash;

        this._expandedSetting = new WebInspector.Setting("canvas-expanded-" + this.folderSettingsKey, false);
        if (this._expandedSetting.value)
            this.expand();
        else
            this.collapse();

        canvas.addEventListener(WebInspector.Canvas.Event.ProgramWasCreated, this._programWasCreated, this);
        canvas.addEventListener(WebInspector.Canvas.Event.ProgramWasDeleted, this._programWasDeleted, this);

        function validateRepresentedObject(representedObject) {
            return representedObject instanceof WebInspector.ShaderProgram;
        }

        function countChildren() {
            return canvas.programs.length;
        }

        this.registerFolderizeSettings("programs", WebInspector.UIString("Programs"), validateRepresentedObject, countChildren.bind(this), WebInspector.ShaderProgramTreeElement);
        this.updateParentStatus();
    }

    // Overrides from TreeElement (Protected).

    onexpand()
    {
        this._expandedSetting.value = true;
    }

    oncollapse()
    {
        // Only store the setting if we have children, since setting hasChildren to false will cause a collapse,
        // and we only care about user triggered collapses.
        if (this.hasChildren)
            this._expandedSetting.value = false;
    }

    onpopulate()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;

        this.shouldRefreshChildren = false;

        this.removeChildren();

        for (var program of this.representedObject.programs)
            this.addChildForRepresentedObject(program);
    }

    // Private

    _programWasCreated(event)
    {
        this.addRepresentedObjectToNewChildQueue(event.data.program);
    }

    _programWasDeleted(event)
    {
        this.removeChildForRepresentedObject(event.data.program);
    }
};
