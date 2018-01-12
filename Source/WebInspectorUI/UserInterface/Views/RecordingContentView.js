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
        this._throttler = this.throttle(200);

        this.element.classList.add("recording", this.representedObject.type);

        let isCanvas2D = this.representedObject.type === WI.Recording.Type.Canvas2D;
        let isCanvasWebGL = this.representedObject.type === WI.Recording.Type.CanvasWebGL;
        if (isCanvas2D || isCanvasWebGL) {
            if (isCanvas2D && WI.RecordingContentView.supportsCanvasPathDebugging()) {
                this._pathContext = null;

                this._showPathButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-path", WI.UIString("Show Path"), WI.UIString("Hide Path"), "Images/Path.svg", 16, 16);
                this._showPathButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showPathButtonClicked, this);
                this._showPathButtonNavigationItem.activated = !!WI.settings.showCanvasPath.value;
            }

            this._showGridButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-grid", WI.UIString("Show Grid"), WI.UIString("Hide Grid"), "Images/NavigationItemCheckers.svg", 13, 13);
            this._showGridButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
            this._showGridButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);
            this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
        }
    }

    // Static

    static supportsCanvasPathDebugging()
    {
        return "currentX" in CanvasRenderingContext2D.prototype && "currentY" in CanvasRenderingContext2D.prototype;
    }

    static _actionModifiesPath(recordingAction)
    {
        switch (recordingAction.name) {
        case "arc":
        case "arcTo":
        case "beginPath":
        case "bezierCurveTo":
        case "closePath":
        case "ellipse":
        case "lineTo":
        case "moveTo":
        case "quadraticCurveTo":
        case "rect":
            return true;
        }

        return false;
    }

    // Public

    get navigationItems()
    {
        let isCanvas2D = this.representedObject.type === WI.Recording.Type.Canvas2D;
        let isCanvasWebGL = this.representedObject.type === WI.Recording.Type.CanvasWebGL;
        if (isCanvas2D || isCanvasWebGL) {
            let navigationItems = [this._showGridButtonNavigationItem];
            if (isCanvas2D && WI.RecordingContentView.supportsCanvasPathDebugging())
                navigationItems.unshift(this._showPathButtonNavigationItem);
            return navigationItems;
        }
        return [];
    }

    updateActionIndex(index, options = {})
    {
        if (!this.representedObject)
            return;

        if (this._index === index)
            return;

        this.representedObject.actions.then((actions) => {
            console.assert(index >= 0 && index < actions.length);
            if (index < 0 || index >= actions.length)
                return;

            this._index = index;
            this._updateSliderValue();

            if (this.representedObject.type === WI.Recording.Type.Canvas2D)
                this._throttler._generateContentCanvas2D(index, actions, options);
            else if (this.representedObject.type === WI.Recording.Type.CanvasWebGL)
                this._throttler._generateContentCanvasWebGL(index, actions, options);
        });
    }

    shown()
    {
        super.shown();

        let isCanvas2D = this.representedObject.type === WI.Recording.Type.Canvas2D;
        let isCanvasWebGL = this.representedObject.type === WI.Recording.Type.CanvasWebGL;
        if (isCanvas2D || isCanvasWebGL) {
            if (isCanvas2D)
                this._updateCanvasPath();
            this._updateImageGrid();
        }
    }

    hidden()
    {
        super.hidden();

        this._generateContentCanvas2D.cancelThrottle();
        this._generateContentCanvasWebGL.cancelThrottle();
    }

    // Protected

    get supportsSave()
    {
        return true;
    }

    get saveData()
    {
        let filename = this.representedObject.displayName;
        if (!filename.endsWith(".json"))
            filename += ".json";

        return {
            url: "web-inspector:///" + encodeURI(filename),
            content: JSON.stringify(this.representedObject.toJSON()),
            forceSaveAs: true,
        };
    }

    // Protected

    initialLayout()
    {
        let previewHeader = this.element.appendChild(document.createElement("header"));

        let sliderContainer = previewHeader.appendChild(document.createElement("div"));
        sliderContainer.className = "slider-container hidden";

        this._previewContainer = this.element.appendChild(document.createElement("div"));
        this._previewContainer.className = "preview-container";

        this._sliderValueElement = sliderContainer.appendChild(document.createElement("div"));
        this._sliderValueElement.className = "slider-value";

        this._sliderElement = sliderContainer.appendChild(document.createElement("input"));
        this._sliderElement.addEventListener("input", this._sliderChanged.bind(this));
        this._sliderElement.type = "range";
        this._sliderElement.min = 0;
        this._sliderElement.max = 0;

        this.representedObject.actions.then(() => {
            sliderContainer.classList.remove("hidden");
            this._sliderElement.max = this.representedObject.visualActionIndexes.length;
            this._updateSliderValue();
        });
    }

    // Private

    async _generateContentCanvas2D(index, actions, options = {})
    {
        let imageLoad = (event) => {
            // Loading took too long and the current action index has already changed.
            if (index !== this._index)
                return;

            this._generateContentCanvas2D(index, actions, options);
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

        let showCanvasPath = WI.RecordingContentView.supportsCanvasPathDebugging() && WI.settings.showCanvasPath.value;
        let indexOfLastBeginPathAction = Infinity;

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
                    if (WI.RecordingAction.isFunctionForType(this.representedObject.type, name))
                        snapshot.context[name](...snapshot.state[name]);
                    else
                        snapshot.context[name] = snapshot.state[name];
                } catch {
                    delete snapshot.state[name];
                }
            }

            let shouldDrawCanvasPath = showCanvasPath && indexOfLastBeginPathAction <= to;
            if (shouldDrawCanvasPath) {
                if (!this._pathContext) {
                    let pathCanvas = document.createElement("canvas");
                    pathCanvas.classList.add("path");
                    this._pathContext = pathCanvas.getContext("2d");
                }

                this._pathContext.canvas.width = snapshot.element.width;
                this._pathContext.canvas.height = snapshot.element.height;
                this._pathContext.clearRect(0, 0, snapshot.element.width, snapshot.element.height);

                this._pathContext.save();

                this._pathContext.fillStyle = "hsla(0, 0%, 100%, 0.75)";
                this._pathContext.fillRect(0, 0, snapshot.element.width, snapshot.element.height);
            }

            let lastPathPoint = {};
            let subPathStartPoint = {};

            for (let i = from; i <= to; ++i) {
                if (actions[i].name === "save")
                    ++saveCount;
                else if (actions[i].name === "restore") {
                    if (!saveCount) // Only attempt to restore if save has been called.
                        continue;
                    --saveCount;
                }

                actions[i].apply(snapshot.context);

                if (shouldDrawCanvasPath && i >= indexOfLastBeginPathAction && WI.RecordingContentView._actionModifiesPath(actions[i])) {
                    lastPathPoint = {x: this._pathContext.currentX, y: this._pathContext.currentY};

                    if (i === indexOfLastBeginPathAction)
                        this._pathContext.setTransform(snapshot.context.getTransform());

                    let isMoveTo = actions[i].name === "moveTo";
                    this._pathContext.lineWidth = isMoveTo ? 0.5 : 1;
                    this._pathContext.setLineDash(isMoveTo ? [5, 5] : []);
                    this._pathContext.strokeStyle = i === to ? "red" : "black";

                    this._pathContext.beginPath();
                    if (!isEmptyObject(lastPathPoint))
                        this._pathContext.moveTo(lastPathPoint.x, lastPathPoint.y);

                    if (actions[i].name === "closePath" && !isEmptyObject(subPathStartPoint)) {
                        this._pathContext.lineTo(subPathStartPoint.x, subPathStartPoint.y);
                        subPathStartPoint = {};
                    } else {
                        actions[i].apply(this._pathContext, {nameOverride: isMoveTo ? "lineTo" : null});
                        if (isMoveTo)
                            subPathStartPoint = {x: this._pathContext.currentX, y: this._pathContext.currentY};
                    }

                    this._pathContext.stroke();
                }
            }

            if (shouldDrawCanvasPath) {
                this._pathContext.restore();
                this._previewContainer.appendChild(this._pathContext.canvas);
            } else if (this._pathContext)
                this._pathContext.canvas.remove();

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
                if (this._snapshots[lastSnapshotIndex])
                    break;
            }

            let startIndex = 0;
            if (lastSnapshotIndex < 0) {
                snapshot.content = this._initialContent;
                snapshot.state = {};

                for (let key in initialState.attributes) {
                    let value = initialState.attributes[key];

                    switch (key) {
                    case "setTransform":
                        value = [await this.representedObject.swizzle(value, WI.Recording.Swizzle.DOMMatrix)];
                        break;

                    case "fillStyle":
                    case "strokeStyle":
                        if (Array.isArray(value)) {
                            let canvasStyle = await this.representedObject.swizzle(value[0], WI.Recording.Swizzle.String);
                            if (canvasStyle.includes("gradient"))
                                value = await this.representedObject.swizzle(value, WI.Recording.Swizzle.CanvasGradient);
                            else if (canvasStyle === "pattern")
                                value = await this.representedObject.swizzle(value, WI.Recording.Swizzle.CanvasPattern);
                        } else
                            value = await this.representedObject.swizzle(value, WI.Recording.Swizzle.String);
                        break;

                    case "direction":
                    case "font":
                    case "globalCompositeOperation":
                    case "imageSmoothingEnabled":
                    case "imageSmoothingQuality":
                    case "lineCap":
                    case "lineJoin":
                    case "shadowColor":
                    case "textAlign":
                    case "textBaseline":
                        value = await this.representedObject.swizzle(value, WI.Recording.Swizzle.String);
                        break;

                    case "setPath":
                        value = [await this.representedObject.swizzle(value[0], WI.Recording.Swizzle.Path2D)];
                        break;
                    }

                    if (value === undefined || (Array.isArray(value) && value.includes(undefined)))
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

                if (WI.RecordingContentView.supportsCanvasPathDebugging())
                    snapshot.state.setPath = [snapshot.context.getPath()];
            });

            snapshot.content = new Image;
            snapshot.content.src = snapshot.element.toDataURL();
            snapshot.content.addEventListener("load", imageLoad);
            return;
        }

        this._previewContainer.removeChildren();

        if (showCanvasPath) {
            indexOfLastBeginPathAction = this._index;
            while (indexOfLastBeginPathAction > snapshot.index && actions[indexOfLastBeginPathAction].name !== "beginPath")
                --indexOfLastBeginPathAction;
        }

        applyActions(snapshot.index, this._index, () => {
            if (options.actionCompletedCallback)
                options.actionCompletedCallback(actions[this._index], snapshot.context);
        });

        this._previewContainer.appendChild(snapshot.element);
        this._updateImageGrid();
    }

    async _generateContentCanvasWebGL(index, actions, options = {})
    {
        let imageLoad = (event) => {
            // Loading took too long and the current action index has already changed.
            if (index !== this._index)
                return;

            this._generateContentCanvasWebGL(index, actions, options);
        };

        let initialState = this.representedObject.initialState;
        if (initialState.content && !this._initialContent) {
            this._initialContent = new Image;
            this._initialContent.src = initialState.content;
            this._initialContent.addEventListener("load", imageLoad);
            return;
        }

        let visualIndex = index;
        while (!actions[visualIndex].isVisual && !(actions[visualIndex] instanceof WI.RecordingInitialStateAction))
            visualIndex--;

        let snapshot = this._snapshots[visualIndex];
        if (!snapshot) {
            if (actions[visualIndex].snapshot) {
                snapshot = this._snapshots[visualIndex] = {element: new Image};
                snapshot.element.src = actions[visualIndex].snapshot;
                snapshot.element.addEventListener("load", imageLoad);
                return;
            }

            if (actions[visualIndex] instanceof WI.RecordingInitialStateAction)
                snapshot = this._snapshots[visualIndex] = {element: this._initialContent};
        }

        if (snapshot) {
            this._previewContainer.removeChildren();
            this._previewContainer.appendChild(snapshot.element);

            this._updateImageGrid();
        }

        if (options.actionCompletedCallback)
            options.actionCompletedCallback(actions[this._index]);
    }

    _updateCanvasPath()
    {
        let activated = WI.settings.showCanvasPath.value;
        if (this._showPathButtonNavigationItem.activated !== activated) {
            this.representedObject.actions.then((actions) => {
                this._generateContentCanvas2D(this._index, actions);
            });
        }

        this._showPathButtonNavigationItem.activated = activated;
    }

    _updateImageGrid()
    {
        let activated = WI.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.activated = activated;

        let snapshotIndex = Math.floor(this._index / WI.RecordingContentView.SnapshotInterval);
        if (!isNaN(this._index) && this._snapshots[snapshotIndex])
            this._snapshots[snapshotIndex].element.classList.toggle("show-grid", activated);
    }

    _updateSliderValue()
    {
        if (!this._sliderElement)
            return;

        let visualActionIndexes = this.representedObject.visualActionIndexes;
        let visualActionIndex = 0;
        if (this._index > 0) {
            while (visualActionIndex < visualActionIndexes.length && visualActionIndexes[visualActionIndex] <= this._index)
                visualActionIndex++;
        }

        this._sliderElement.value = visualActionIndex;
        this._sliderValueElement.textContent = WI.UIString("%d of %d").format(visualActionIndex, visualActionIndexes.length);
    }

    _showPathButtonClicked(event)
    {
        WI.settings.showCanvasPath.value = !this._showPathButtonNavigationItem.activated;

        this._updateCanvasPath();
    }

    _showGridButtonClicked(event)
    {
        WI.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;

        this._updateImageGrid();
    }

    _sliderChanged()
    {
        let index = 0;

        let visualActionIndex = parseInt(this._sliderElement.value) - 1;
        if (visualActionIndex !== -1)
            index = this.representedObject.visualActionIndexes[visualActionIndex];

        this.dispatchEventToListeners(WI.RecordingContentView.Event.RecordingActionIndexChanged, {index});
    }
};

WI.RecordingContentView.SnapshotInterval = 5000;

WI.RecordingContentView.Event = {
    RecordingActionIndexChanged: "recording-action-index-changed",
};
