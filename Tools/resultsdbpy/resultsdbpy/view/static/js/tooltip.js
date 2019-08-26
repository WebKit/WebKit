// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import {DOM, REF} from '/library/js/Ref.js';

function isPointInElement(element, point)
{
    if (!element || element.style.display == 'none')
        return false;
    const bounds = element.getBoundingClientRect();
    return point.x >= bounds.left && point.x <= bounds.right && point.y >= bounds.top && point.y <= bounds.bottom;
}

class _ToolTip {
    constructor() {
        this.ref = null;
        this.arrow = null;
        this.onArrowClick = null;
    }
    toString() {
        const self = this;
        this.ref = REF.createRef({
            state: {content: null, points: null},
            onElementMount: (element) => {
                element.addEventListener('mouseleave', (event) => {
                    if (!isPointInElement(self.arrow.element, event))
                        this.unset()
                });
            },
            onStateUpdate: (element, stateDiff, state) => {
                if (stateDiff.content) {
                    DOM.inject(element, stateDiff.content);
                    element.style.display = null;
                }
                if (!state.content && !element.style.display) {
                    element.style.display = 'none';
                    DOM.inject(element, '');
                }
                if (stateDiff.points) {
                    element.style.left = '0px';
                    element.style.top = '0px';

                    const upperPoint = stateDiff.points.length > 1 && stateDiff.points[0].y > stateDiff.points[1].y ? stateDiff.points[1] : stateDiff.points[0];
                    const lowerPoint = stateDiff.points.length > 1 && stateDiff.points[1].y > stateDiff.points[0].y ? stateDiff.points[1] : stateDiff.points[0];
                    const scrollDelta = document.documentElement.scrollTop || document.body.scrollTop;
                    const bounds = element.getBoundingClientRect();
                    const viewportWitdh = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
                    const viewportHeight = Math.max(document.documentElement.clientHeight, window.innerHeight || 0);

                    // Make an effort to place the tooltip in the center of the viewport.
                    let direction = 'down';
                    let tipY = upperPoint.y - 8 - bounds.height;
                    let point = upperPoint;
                    if (tipY < scrollDelta || tipY + bounds.height + (lowerPoint.y - upperPoint.y) / 2 < scrollDelta + viewportHeight / 2) {
                        direction = 'up';
                        tipY = lowerPoint.y + 16;
                        point = lowerPoint;
                    }
                    element.style.top = `${tipY}px`;

                    let tipX = point.x - bounds.width / 2;
                    if (tipX + bounds.width > viewportWitdh)
                        tipX = viewportWitdh - bounds.width;
                    if (tipX < 0)
                        tipX = 0;
                    element.style.left = `${tipX}px`;

                    self.arrow.setState({direction: direction, location: point});
                }
            },
        });
        this.arrow = REF.createRef({
            state: {direction: null, location: null},
            onElementMount: (element) => {
                element.addEventListener('mouseleave', (event) => {
                    if (!isPointInElement(self.ref.element, event))
                        this.unset()
                });
            },
            onStateUpdate: (element, stateDiff, state) => {
                if (!state.direction || !state.location) {
                    element.style.display = 'none';
                    element.onclick = null;
                    element.style.cursor = null;
                    return;
                }

                if (self.onArrowClick) {
                    element.onclick = self.onArrowClick;
                    element.style.cursor = 'pointer';
                } else {
                    element.onclick = null;
                    element.style.cursor = null;
                }

                element.classList = [`tooltip arrow-${state.direction}`];
                element.style.left = `${state.location.x - 15}px`;
                if (state.direction == 'down')
                    element.style.top = `${state.location.y - 8}px`;
                else
                    element.style.top = `${state.location.y - 13}px`;
                element.style.display = null;
            },

        });
        return `<div class="tooltip arrow-up" ref="${this.arrow}"></div>
            <div class="tooltip-content" ref="${this.ref}">
            </div>`;
    }
    set(content, points, onArrowClick = null) {
        if (!this.ref) {
            console.error('Cannot set ToolTip content, no tooltip on the page');
            return;
        }
        if (!points || points.length == 0) {
            console.error('Tool tips require a location');
            return;
        }
        this.onArrowClick = onArrowClick;
        this.ref.setState({content: content, points: points});
    }
    unset() {
        if (this.ref)
            this.ref.setState({content: null, points: null});
        if (this.arrow)
            this.arrow.setState({direction: null, points: null});
    }
    isIn(point) {
        return isPointInElement(this.ref.element, point) || isPointInElement(this.arrow.element, point);
    }
}

const ToolTip = new _ToolTip();

export {ToolTip};
