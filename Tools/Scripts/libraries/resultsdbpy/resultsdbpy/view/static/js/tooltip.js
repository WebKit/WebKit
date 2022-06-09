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
    return point.x >= bounds.left - 1 && point.x <= bounds.right + 1 && point.y >= bounds.top - 1 && point.y <= bounds.bottom + 1;
}

class _ToolTip {
    constructor() {
        this.ref = null;
        this.arrow = null;
        this.onArrowClick = null;

        this.VERTICAL = 0;
        this.HORIZONTAL = 1;
    }
    toString() {
        const self = this;
        this.ref = REF.createRef({
            state: {content: null, points: null, viewport: null},
            onElementMount: (element) => {
                element.addEventListener('mouseleave', (event) => {
                    if (element.style.display === 'none')
                        return;
                    if (!isPointInElement(self.arrow.element, event))
                        this.unset()
                });
            },
            onStateUpdate: (element, stateDiff, state) => {
                if (stateDiff.content) {
                    DOM.inject(element, stateDiff.content);
                    element.style.display = null;
                } else {
                    element.style.display = 'none';
                    DOM.inject(element, '');
                }

                if (stateDiff.points || stateDiff.viewport) {
                    element.style.left = '0px';
                    element.style.top = '0px';

                    const upperPoint = stateDiff.points.length > 1 && stateDiff.points[0].y > stateDiff.points[1].y ? stateDiff.points[1] : stateDiff.points[0];
                    const lowerPoint = stateDiff.points.length > 1 && stateDiff.points[1].y > stateDiff.points[0].y ? stateDiff.points[1] : stateDiff.points[0];
                    const scrollDelta = document.documentElement.scrollTop || document.body.scrollTop;
                    const bounds = element.getBoundingClientRect();

                    let direction = 'down';
                    let point = upperPoint;

                    if (upperPoint.y == lowerPoint.y) {
                        // Horizontal tooltip
                        const leftPoint = stateDiff.points.length > 1 && stateDiff.points[0].x > stateDiff.points[1].x ? stateDiff.points[1] : stateDiff.points[0];
                        const rightPoint = stateDiff.points.length > 1 && stateDiff.points[1].x > stateDiff.points[0].x ? stateDiff.points[1] : stateDiff.points[0];

                        direction = 'left';
                        let tipX = leftPoint.x - 12 - bounds.width;
                        point = rightPoint;
                        if (tipX < 0 || tipX + bounds.width + (rightPoint.x - leftPoint.x) / 2 < stateDiff.viewport.x + stateDiff.viewport.width / 2) {
                            direction = 'right';
                            tipX = rightPoint.x + 16;
                            point = rightPoint;
                        }
                        element.style.left = `${tipX}px`;

                        let tipY = point.y - bounds.height / 2;
                        if (tipY + bounds.height > scrollDelta + stateDiff.viewport.y + stateDiff.viewport.height)
                            tipY = scrollDelta + stateDiff.viewport.y + stateDiff.viewport.height - bounds.height;
                        if (tipY < 0)
                            tipY = 0;
                        element.style.top = `${tipY}px`;
                    } else {
                        // Make an effort to place the tooltip in the center of the viewport.
                        let tipY = upperPoint.y - 8 - bounds.height;
                        point = upperPoint;
                        if (tipY < scrollDelta || tipY + bounds.height + (lowerPoint.y - upperPoint.y) / 2 < scrollDelta + stateDiff.viewport.y + stateDiff.viewport.height / 2) {
                            direction = 'up';
                            tipY = lowerPoint.y + 16;
                            point = lowerPoint;
                        }
                        element.style.top = `${tipY}px`;

                        let tipX = point.x - bounds.width / 2;
                        if (tipX + bounds.width > stateDiff.viewport.x + stateDiff.viewport.width)
                            tipX = stateDiff.viewport.x + stateDiff.viewport.width - bounds.width;
                        if (tipX < 0)
                            tipX = 0;
                        element.style.left = `${tipX}px`;
                    }

                    self.arrow.setState({direction: direction, location: point});
                }
            },
        });
        this.arrow = REF.createRef({
            state: {direction: null, location: null},
            onElementMount: (element) => {
                element.addEventListener('mouseleave', (event) => {
                    if (element.style.display === 'none')
                        return;
                    if (!isPointInElement(self.ref.element, event) && !isPointInElement(element, event))
                        this.unset()
                });
            },
            onStateUpdate: (element, stateDiff) => {
                if (!stateDiff.direction || !stateDiff.location) {
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

                element.classList = [`tooltip arrow-${stateDiff.direction}`];
                
                if (stateDiff.direction == 'down') {
                    element.style.left = `${stateDiff.location.x - 15}px`;
                    element.style.top = `${stateDiff.location.y - 8}px`;
                } else if (stateDiff.direction == 'left') {
                    element.style.left = `${stateDiff.location.x - 30}px`;
                    element.style.top = `${stateDiff.location.y - 15}px`;
                } else if (stateDiff.direction == 'right') {
                    element.style.left = `${stateDiff.location.x - 13}px`;
                    element.style.top = `${stateDiff.location.y - 15}px`;
                } else {
                    element.style.left = `${stateDiff.location.x - 15}px`;
                    element.style.top = `${stateDiff.location.y - 13}px`;
                }
                element.style.display = null;
            },

        });
        return `<div class="tooltip arrow-up" ref="${this.arrow}"></div>
            <div class="tooltip-content" ref="${this.ref}">
            </div>`;
    }
    set(content, points, onArrowClick = null, viewport = null) {
        if (!this.ref) {
            console.error('Cannot set ToolTip content, no tooltip on the page');
            return;
        }
        if (!points || points.length == 0) {
            console.error('Tool tips require a location');
            return;
        }
        this.onArrowClick = onArrowClick;

        const windowWidth = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
        const windowHeight = Math.max(document.documentElement.clientHeight, window.innerHeight || 0);
        if (!viewport)
            viewport = {
                x: 0,
                y: 0,
                width: windowWidth,
                height: windowHeight,
            }
        else {
            let rect = viewport.getBoundingClientRect();
            viewport = {
                x: 0,
                y: 0,
                width: Math.min(windowWidth, rect.width),
                height: Math.min(windowHeight, rect.height),
            };
        }
        this.ref.setState({content: content, points: points, viewport: viewport});
    }
    setByElement(content, element, options, viewport = null) {
        const bound = element.getBoundingClientRect();
        const orientation = options.orientation ? options.orientation : this.VERTICAL;
        const onArrowClick = options.onArrowClick ? options.onArrowClick : null;

        // Manage the scroll delta
        let scrollDelta = 0;
        if (window.getComputedStyle(element.offsetParent).getPropertyValue('position') == 'fixed')
            scrollDelta = document.documentElement.scrollTop || document.body.scrollTop;

        if (options.orientation) {
            this.set(content, [
                {x: bound.right, y: (bound.top + bound.bottom) / 2 + scrollDelta},
                {x: bound.left, y: (bound.top + bound.bottom) / 2 + scrollDelta},
            ], onArrowClick, viewport);
        } else {
            this.set(content, [
                {x: (bound.right + bound.left) / 2, y: bound.top + scrollDelta},
                {x: (bound.right + bound.left) / 2, y: bound.bottom + scrollDelta},
            ], onArrowClick, viewport);
        }
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
