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

WI.RecordingContentView = class RecordingContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.Recording);

        super(representedObject);

        this._index = NaN;
        this._snapshots = [];
        this._initialContent = null;

        this.element.classList.add("recording", this.representedObject.type);

        if (this.representedObject.type === WI.Recording.Type.Canvas2D) {
            this._showGridButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-grid", WI.UIString("Show Grid"), WI.UIString("Hide Grid"), "Images/NavigationItemCheckers.svg", 13, 13);
            this._showGridButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);
            this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
        }

        this._previewContainer = this.element.appendChild(document.createElement("div"));
        this._previewContainer.classList.add("preview-container");
}

    // Public

    get navigationItems()
    {
        if (this.representedObject.type === WI.Recording.Type.Canvas2D)
            return [this._showGridButtonNavigationItem];
        return [];
    }

    updateActionIndex(index, options = {})
    {
        console.assert(!this.representedObject || (index >= 0 && index < this.representedObject.actions.length));
        if (!this.representedObject || index < 0 || index >= this.representedObject.actions.length || index === this._index)
            return;

        this._index = index;

        if (this.representedObject.type === WI.Recording.Type.Canvas2D)
            this._generateContentCanvas2D(index, options);
    }

    // Protected

    shown()
    {
        super.shown();

        if (this.representedObject.type === WI.Recording.Type.Canvas2D)
            this._updateImageGrid();
    }

    get supplementalRepresentedObjects()
    {
        let supplementalRepresentedObjects = super.supplementalRepresentedObjects;
        if (this.representedObject)
            supplementalRepresentedObjects.push(this.representedObject);
        return supplementalRepresentedObjects;
    }

    // Private

    _generateContentCanvas2D(index, options = {})
    {
        let imageLoad = (event) => {
            // Loading took too long and the current action index has already changed.
            if (index !== this._index)
                return;

            this._generateContentCanvas2D(index, options);
        };

        let initialState = this.representedObject.initialState;
        if (initialState.content && !this._initialContent) {
            this._initialContent = new Image;
            this._initialContent.src = initialState.content;
            this._initialContent.addEventListener("load", imageLoad);
            return;
        }

        let snapshotIndex = Math.floor(index / WI.RecordingContentView.SnapshotInterval);
        let snapshot = this._snapshots[snapshotIndex];

        let actions = this.representedObject.actions;
        let applyActions = (from, to, callback) => {
            let saveCount = 0;
            snapshot.context.save();

            if (snapshot.content) {
                snapshot.context.clearRect(0, 0, snapshot.element.width, snapshot.element.height);
                snapshot.context.drawImage(snapshot.content, 0, 0);
            }

            for (let name in snapshot.state) {
                if (!(name in snapshot.context))
                    continue;

                try {
                    if (WI.RecordingAction.functionForType(this.representedObject.type, name))
                        snapshot.context[name](...snapshot.state[name]);
                    else
                        snapshot.context[name] = snapshot.state[name];
                } catch (e) {
                    delete snapshot.state[name];
                }
            }

            for (let i = from; i <= to; ++i) {
                if (actions[i].name === "save")
                    ++saveCount;
                else if (actions[i].name === "restore") {
                    if (!saveCount) // Only attempt to restore if save has been called.
                        continue;
                    --saveCount;
                }

                this._applyAction(snapshot.context, actions[i]);
            }

            callback();

            snapshot.context.restore();
            while (saveCount-- > 0)
                snapshot.context.restore();
        };

        if (!snapshot) {
            snapshot = this._snapshots[snapshotIndex] = {};
            snapshot.index = snapshotIndex * WI.RecordingContentView.SnapshotInterval;
            while (snapshot.index && actions[snapshot.index].name !== "beginPath")
                --snapshot.index;

            snapshot.element = document.createElement("canvas");
            snapshot.context = snapshot.element.getContext("2d", ...initialState.parameters);
            if ("width" in initialState.attributes)
                snapshot.element.width = initialState.attributes.width;
            if ("height" in initialState.attributes)
                snapshot.element.height = initialState.attributes.height;

            let lastSnapshotIndex = snapshotIndex;
            while (--lastSnapshotIndex >= 0) {
                if (this._snapshots[--lastSnapshotIndex])
                    break;
            }

            let startIndex = 0;
            if (lastSnapshotIndex < 0) {
                snapshot.content = this._initialContent;
                snapshot.state = {};

                for (let key in initialState.attributes) {
                    let value = initialState.attributes[key];
                    if (key === "strokeStyle" || key === "fillStyle")
                        value = this.representedObject.swizzle(value, WI.Recording.Swizzle.CanvasStyle);

                    if (value === WI.Recording.Swizzle.Invalid)
                        continue;

                    snapshot.state[key] = value;
                }
            } else {
                snapshot.content = this._snapshots[lastSnapshotIndex].content;
                snapshot.state = this._snapshots[lastSnapshotIndex].state;
                startIndex = this._snapshots[lastSnapshotIndex].index;
            }

            applyActions(startIndex, snapshot.index - 1, () => {
                snapshot.state = {
                    direction: snapshot.context.direction,
                    fillStyle: snapshot.context.fillStyle,
                    font: snapshot.context.font,
                    globalAlpha: snapshot.context.globalAlpha,
                    globalCompositeOperation: snapshot.context.globalCompositeOperation,
                    imageSmoothingEnabled: snapshot.context.imageSmoothingEnabled,
                    imageSmoothingQuality: snapshot.context.imageSmoothingQuality,
                    lineCap: snapshot.context.lineCap,
                    lineDashOffset: snapshot.context.lineDashOffset,
                    lineJoin: snapshot.context.lineJoin,
                    lineWidth: snapshot.context.lineWidth,
                    miterLimit: snapshot.context.miterLimit,
                    setLineDash: [snapshot.context.getLineDash()],
                    setTransform: [snapshot.context.getTransform()],
                    shadowBlur: snapshot.context.shadowBlur,
                    shadowColor: snapshot.context.shadowColor,
                    shadowOffsetX: snapshot.context.shadowOffsetX,
                    shadowOffsetY: snapshot.context.shadowOffsetY,
                    strokeStyle: snapshot.context.strokeStyle,
                    textAlign: snapshot.context.textAlign,
                    textBaseline: snapshot.context.textBaseline,
                    webkitImageSmoothingEnabled: snapshot.context.webkitImageSmoothingEnabled,
                    webkitLineDash: snapshot.context.webkitLineDash,
                    webkitLineDashOffset: snapshot.context.webkitLineDashOffset,
                };
            });

            snapshot.content = new Image;
            snapshot.content.src = snapshot.element.toDataURL();
            snapshot.content.addEventListener("load", imageLoad);
            return;
        }

        this._previewContainer.removeChildren();

        applyActions(snapshot.index, this._index, () => {
            if (options.onCompleteCallback)
                options.onCompleteCallback(snapshot.context);
        });

        this._previewContainer.appendChild(snapshot.element);
        this._updateImageGrid();
    }

    _applyAction(context, action, options = {})
    {
        if (!action.valid)
            return;

        if (action.parameters.includes(WI.Recording.Swizzle.Invalid))
            return;

        try {
            let name = options.nameOverride || action.name;
            if (action.isFunction)
                context[name](...action.parameters);
            else {
                if (action.isGetter)
                    context[name];
                else
                    context[name] = action.parameters[0];
            }
        } catch (e) {
            WI.Recording.synthesizeError(WI.UIString("“%s” threw an error.").format(action.name));

            action.valid = false;
        }
    }

    _updateImageGrid()
    {
        let activated = WI.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.activated = activated;

        let snapshotIndex = Math.floor(this._index / WI.RecordingContentView.SnapshotInterval);
        if (!isNaN(this._index) && this._snapshots[snapshotIndex])
            this._snapshots[snapshotIndex].element.classList.toggle("show-grid", activated);
    }

    _showGridButtonClicked(event)
    {
        WI.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;

        this._updateImageGrid();
    }
};

WI.RecordingContentView.SnapshotInterval = 5000;
