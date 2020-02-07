/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

WI.AnimationContentView = class AnimationContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.Animation);

        super(representedObject);

        this._animationTargetDOMNode = null;
        this._cachedWidth = NaN;

        this.element.classList.add("animation");
    }

    // Static

    static get previewHeight()
    {
        return 40;
    }

    // Public

    handleRefreshButtonClicked()
    {
        this._refreshSubtitle();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let headerElement = this.element.appendChild(document.createElement("header"));

        let titlesContainer = headerElement.appendChild(document.createElement("div"));
        titlesContainer.className = "titles";

        let titleElement = titlesContainer.appendChild(document.createElement("span"));
        titleElement.className = "title";
        titleElement.textContent = this.representedObject.displayName;

        this._subtitleElement = titlesContainer.appendChild(document.createElement("span"));
        this._subtitleElement.className = "subtitle";

        let navigationBar = new WI.NavigationBar;

        let animationTargetButtonNavigationItem = new WI.ButtonNavigationItem("animation-target", WI.UIString("Animation Target"), "Images/Markup.svg", 16, 16);
        animationTargetButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        WI.addMouseDownContextMenuHandlers(animationTargetButtonNavigationItem.element, this._populateAnimationTargetButtonContextMenu.bind(this));
        navigationBar.addNavigationItem(animationTargetButtonNavigationItem);

        headerElement.append(navigationBar.element);
        this.addSubview(navigationBar);

        this._previewContainer = this.element.appendChild(document.createElement("div"));
        this._previewContainer.className = "preview";
    }

    layout()
    {
        super.layout();

        this._refreshSubtitle();
        this._refreshPreview();
    }

    sizeDidChange()
    {
        super.sizeDidChange();

        this._cachedWidth = this.element.realOffsetWidth;
    }

    attached()
    {
        super.attached();

        this.representedObject.addEventListener(WI.Animation.Event.EffectChanged, this._handleEffectChanged, this);
        this.representedObject.addEventListener(WI.Animation.Event.TargetChanged, this._handleTargetChanged, this);
    }

    detached()
    {
        this.representedObject.removeEventListener(WI.Animation.Event.TargetChanged, this._handleTargetChanged, this);
        this.representedObject.removeEventListener(WI.Animation.Event.EffectChanged, this._handleEffectChanged, this);

        super.detached();
    }

    // Private

    _refreshSubtitle()
    {
        this.representedObject.requestEffectTarget((domNode) => {
            this._animationTargetDOMNode = domNode;

            this._subtitleElement.removeChildren();
            if (domNode)
                this._subtitleElement.appendChild(WI.linkifyNodeReference(domNode));
        });
    }

    _refreshPreview()
    {
        this._previewContainer.removeChildren();

        let keyframes = this.representedObject.keyframes;
        if (!keyframes.length) {
            let span = this._previewContainer.appendChild(document.createElement("span"));
            span.textContent = WI.UIString("This animation has no keyframes.");
            return;
        }

        let startDelay = this.representedObject.startDelay || 0;
        let iterationDuration = this.representedObject.iterationDuration || 0;
        let endDelay = this.representedObject.endDelay || 0;
        let totalDuration = startDelay + iterationDuration + endDelay;
        if (totalDuration === 0) {
            let span = this._previewContainer.appendChild(document.createElement("span"));
            span.textContent = WI.UIString("This animation has no duration.");
            return;
        }

        const previewHeight = WI.AnimationContentView.previewHeight;

        const markerHeadRadius = 4;
        const markerHeadPadding = 2;

        // Squeeze the entire preview so that markers aren't cut off.
        const squeezeXStart = (iterationDuration && startDelay) ? 0 : markerHeadRadius + markerHeadPadding;
        const squeezeXEnd = (iterationDuration && endDelay) ? 0 : markerHeadRadius + markerHeadPadding;
        const squeezeYStart = markerHeadRadius + (markerHeadPadding * 2);

        // Move the easing line down to cut off the bottom border.
        const adjustEasingY = 0.5;

        let secondsPerPixel = this._cachedWidth / totalDuration;

        startDelay *= secondsPerPixel;
        iterationDuration = (iterationDuration * secondsPerPixel) - squeezeXStart - squeezeXEnd;
        endDelay *= secondsPerPixel;

        let svg = this._previewContainer.appendChild(createSVGElement("svg"));
        svg.setAttribute("viewBox", `0 0 ${this._cachedWidth} ${previewHeight}`);

        function addTitle(parent, title) {
            let titleElement = parent.appendChild(createSVGElement("title"));
            titleElement.textContent = title;
        }

        if (startDelay) {
            let startDelayContainer = svg.appendChild(createSVGElement("g"));
            startDelayContainer.classList.add("delay", "start");

            let startDelayLine = startDelayContainer.appendChild(createSVGElement("line"));
            startDelayLine.setAttribute("y1", (previewHeight + squeezeYStart) / 2);
            startDelayLine.setAttribute("x2", startDelay);
            startDelayLine.setAttribute("y2", (previewHeight + squeezeYStart) / 2);

            let startDelayElement = startDelayContainer.appendChild(createSVGElement("rect"));
            startDelayElement.setAttribute("width", startDelay);
            startDelayElement.setAttribute("height", previewHeight);

            const startDelayTitleFormat = WI.UIString("Start Delay %s", "Web Animation Start Delay Tooltip", "Tooltip for section of graph representing delay before a web animation begins applying styles");
            addTitle(startDelayElement, startDelayTitleFormat.format(Number.secondsToString(this.representedObject.startDelay / 1000)));
        }

        if (endDelay) {
            let endDelayContainer = svg.appendChild(createSVGElement("g"));
            endDelayContainer.setAttribute("transform", `translate(${startDelay + iterationDuration + squeezeXStart}, 0)`);
            endDelayContainer.classList.add("delay", "end");

            let endDelayLine = endDelayContainer.appendChild(createSVGElement("line"));
            endDelayLine.setAttribute("y1", (previewHeight + squeezeYStart) / 2);
            endDelayLine.setAttribute("x2", endDelay);
            endDelayLine.setAttribute("y2", (previewHeight + squeezeYStart) / 2);

            let endDelayElement = endDelayContainer.appendChild(createSVGElement("rect"));
            endDelayElement.setAttribute("width", startDelay + iterationDuration + endDelay);
            endDelayElement.setAttribute("height", previewHeight);

            const endDelayTitleFormat = WI.UIString("End Delay %s", "Web Animation End Delay Tooltip", "Tooltip for section of graph representing delay after a web animation finishes applying styles");
            addTitle(endDelayElement, endDelayTitleFormat.format(Number.secondsToString(this.representedObject.endDelay / 1000)));
        }

        if (iterationDuration) {
            let timingFunction = this.representedObject.timingFunction;

            let activeDurationContainer = svg.appendChild(createSVGElement("g"));
            activeDurationContainer.classList.add("active");
            activeDurationContainer.setAttribute("transform", `translate(${startDelay + squeezeXStart}, ${squeezeYStart})`);

            const startY = 0;
            const endY = previewHeight - squeezeYStart;
            const height = endY - startY;

            for (let [keyframeA, keyframeB] of keyframes.adjacencies()) {
                let startX = iterationDuration * keyframeA.offset;
                let endX = iterationDuration * keyframeB.offset;
                let width = endX - startX;

                let easing = keyframeA.easing || timingFunction;

                let easingContainer = activeDurationContainer.appendChild(createSVGElement("g"));
                easingContainer.classList.add("easing");
                easingContainer.setAttribute("transform", `translate(${startX}, ${startY})`);

                let x1 = easing.inPoint.x * width;
                let y1 = ((1 - easing.inPoint.y) * height) + adjustEasingY;
                let x2 = easing.outPoint.x * width;
                let y2 = ((1 - easing.outPoint.y) * height) + adjustEasingY;

                let easingPath = easingContainer.appendChild(createSVGElement("path"));
                easingPath.setAttribute("d", `M 0 ${height + adjustEasingY} C ${x1} ${y1} ${x2} ${y2} ${width} ${adjustEasingY} V ${height + adjustEasingY} Z`);

                let titleRect = easingContainer.appendChild(createSVGElement("rect"));
                titleRect.setAttribute("width", width);
                titleRect.setAttribute("height", height);

                addTitle(titleRect, easing.toString());
            }

            for (let keyframe of keyframes) {
                let x = iterationDuration * keyframe.offset;

                let keyframeContainer = activeDurationContainer.appendChild(createSVGElement("g"));
                keyframeContainer.classList.add("keyframe");
                keyframeContainer.setAttribute("transform", `translate(${x}, ${startY})`);

                let keyframeMarkerHead = keyframeContainer.appendChild(createSVGElement("circle"));
                keyframeMarkerHead.setAttribute("r", markerHeadRadius);

                let keyframeMarkerLine = keyframeContainer.appendChild(createSVGElement("line"));
                keyframeMarkerLine.setAttribute("y1", height);

                let titleRect = keyframeContainer.appendChild(createSVGElement("rect"));
                titleRect.setAttribute("x", -1 * (markerHeadRadius + markerHeadPadding));
                titleRect.setAttribute("y", -1 * squeezeYStart);
                titleRect.setAttribute("width", (markerHeadRadius + markerHeadPadding) * 2);
                titleRect.setAttribute("height", height + squeezeYStart);

                addTitle(titleRect, keyframe.style);
            }
        }
    }

    _handleEffectChanged(event)
    {
        this._refreshPreview();
    }

    _handleTargetChanged(event)
    {
        this._refreshSubtitle();
    }

    _populateAnimationTargetButtonContextMenu(contextMenu)
    {
        contextMenu.appendItem(WI.UIString("Log Animation"), () => {
            WI.RemoteObject.resolveAnimation(this.representedObject, WI.RuntimeManager.ConsoleObjectGroup, (remoteObject) => {
                if (!remoteObject)
                    return;

                const text = WI.UIString("Selected Animation", "Appears as a label when a given web animation is logged to the Console");
                const addSpecialUserLogClass = true;
                WI.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, addSpecialUserLogClass);
            });
        });

        contextMenu.appendSeparator();

        if (this._animationTargetDOMNode)
            WI.appendContextMenuItemsForDOMNode(contextMenu, this._animationTargetDOMNode);
    }
};
