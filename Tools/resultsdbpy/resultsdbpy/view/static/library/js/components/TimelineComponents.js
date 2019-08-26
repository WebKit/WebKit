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
    DOM, REF, FP, EventStream
}
from '../Ref.js';

import {isDarkMode, createInsertionObservers} from '../Utils.js';
import {ListComponent, ListProvider, ListProviderReceiver} from './BaseComponents.js'

function pointCircleCollisionDetact(point, circle) {
    return Math.pow(point.x - circle.x, 2) + Math.pow(point.y - circle.y, 2) <= circle.radius * circle.radius;
}

function pointRectCollisionDetect(point, rect) {
    const diffX = point.x - rect.topLeftX;
    const diffY = point.y - rect.topLeftY;
    return diffX <= rect.width && diffY <= rect.height && diffX >= 0 && diffY >= 0;
}

function pointPolygonCollisionDetect(point, polygon) {
    let res = false;
    for (let i = 0, j = 1; i < polygon.length; i++, j = i + 1) {
        if (j === polygon.length )
            j = 0;
        if (pointRightRayLineSegmentCollisionDetect(point, polygon[i], polygon[j]))
            res = !res;
    }
    return res;
}

/*
* Detact if point right ray have a collision with a line segment
*                *
*               /
*        *---> /
*             /
*            *
*/
function pointRightRayLineSegmentCollisionDetect(point, lineStart, lineEnd) {
    const maxX = Math.max(lineStart.x, lineEnd.x);
    const minX = Math.min(lineStart.x, lineEnd.x);
    const maxY = Math.max(lineStart.y, lineEnd.y);
    const minY = Math.min(lineStart.y, lineEnd.y);
    if ((point.x <= maxX && point.x >= minX || point.x < minX) &&
        point.y < maxY && point.y > minY &&
        lineStart.y !== lineEnd.y) {
        const tanTopAngle = (lineEnd.x - lineStart.x) / (lineEnd.y - lineStart.y);
        return point.x < lineEnd.x - tanTopAngle * (lineEnd.y - point.y);
    }
    return false;
}

function getMousePosInCanvas(event, canvas) {
    const rect = canvas.getBoundingClientRect();
    return {
        x: event.clientX - rect.left,
        y: event.clientY - rect.top,
    }
}

function getDevicePixelRatio() {
    return window.devicePixelRatio;
}

function setupCanvasWidthWithDpr(canvas, width) {
    const dpr = getDevicePixelRatio();
    canvas.style.width = `${width}px`;
    canvas.width = width * dpr;
    canvas.logicWidth = width;
}

function setupCanvasHeightWithDpr(canvas, height) {
    const dpr = getDevicePixelRatio();
    canvas.style.height = `${height}px`;
    canvas.height = height * dpr;
    canvas.logicHeight = height;
}

function setupCanvasContextScale(canvas) {
    const dpr = getDevicePixelRatio();
    const context = canvas.getContext('2d');
    context.scale(dpr, dpr);
}

function XScrollableCanvasProvider(exporter, ...childrenFunctions) {
    const containerRef = REF.createRef({
        state: {width: 0},
        onStateUpdate: (element, stateDiff, state) => {
            if (stateDiff.width)
                element.style.width = `${stateDiff.width}px`;
        },
    });
    const scrollRef = REF.createRef({});
    const scrollEventStream = scrollRef.fromEvent('scroll');
    const resizeEventStream = new EventStream();
    window.addEventListener('resize', () => {
        presenterRef.setState({resize:true});
    });
    const resizeContainerWidth = width => {containerRef.setState({width: width})};
    const presenterRef = REF.createRef({
        state: {scrollLeft: 0},
        onElementMount: (element) => {
            element.style.width = `${element.parentElement.parentElement.offsetWidth}px`;
            resizeEventStream.add(element.offsetWidth);
        },
        onStateUpdate: (element, stateDiff, state) => {
            if (stateDiff.resize) {
                element.style.width = `${element.parentElement.parentElement.offsetWidth}px`;
                resizeEventStream.add(element.offsetWidth);
            }
        }
    });
    // Provide parent functions/event to children to use

    return `<div class="content" ref="${scrollRef}">
        <div ref="${containerRef}" style="position: relative">
            <div ref="${presenterRef}" style="position: -webkit-sticky; position:sticky; top:0; left: 0">${
                ListProvider((updateChildrenFunctions) => {
                    if (exporter) {
                        exporter((children) => {
                            updateChildrenFunctions(children);
                            // this make sure the newly added children receive current state
                            resizeEventStream.replayLast();
                            scrollEventStream.replayLast();
                        });
                    }
                }, [resizeContainerWidth, scrollEventStream, resizeEventStream], ...childrenFunctions)
            }</div>
        </div>
    </div>`;
}

class ColorBatchRender {
    constructor() {
        this.colorSeqsMap = {};
    }

    lazyCreateColorSeqs(color, startAction, finalAction) {
        if (false === color in this.colorSeqsMap)
            this.colorSeqsMap[color] = [startAction, finalAction];
    }

    addSeq(color, seqAction) {
        this.colorSeqsMap[color].push(seqAction);
    }

    batchRender(context) {
        for (let color of Object.keys(this.colorSeqsMap)) {
            const seqs = this.colorSeqsMap[color];
            seqs[0](context, color);
            for(let i = 2; i < seqs.length; i++)
                seqs[i](context, color);
            seqs[1](context, color);
        }
    }
    clear() {
        this.colorSeqsMap = new Map();
    }
}

function xScrollStreamRenderFactory(height) {
    return (redraw, element, stateDiff, state) => {
        const width = typeof stateDiff.width === 'number' ? stateDiff.width : state.width;
        if (width <= 0)
            // Nothing to render
            return;
        let startX = 0;
        let renderWidth = width;
        requestAnimationFrame(() => {
            if (element.logicWidth !== width) {
                setupCanvasWidthWithDpr(element, width);
                setupCanvasContextScale(element);
            }
            if (element.logicHeight !== height) {
                setupCanvasHeightWithDpr(element, height);
                setupCanvasContextScale(element);
            }
            element.getContext("2d", {alpha: false}).clearRect(startX, 0, renderWidth, element.logicHeight);
            redraw(startX, renderWidth, element, stateDiff, state);
        });
    }
}

// namespace Timeline
const Timeline = {};

Timeline.CanvasSeriesComponent = (dots, scales, option = {}) => {
    console.assert(dots.length <= scales.length);

    // Get the css value, this component assume to use with webkit.css
    const computedStyle = getComputedStyle(document.body);
    let radius = parseInt(computedStyle.getPropertyValue('--smallSize')) / 2;
    let dotMargin = parseInt(computedStyle.getPropertyValue('--tinySize')) / 2;
    let fontFamily = computedStyle.getPropertyValue('font-family');
    let defaultDotColor = computedStyle.getPropertyValue('--greenLight').trim();
    let defaultEmptyLineColor = computedStyle.getPropertyValue('--grey').trim();
    let defaultInnerLableColor = computedStyle.getPropertyValue('--white').trim();
    let defaultFontSize = 10;

    // Get configuration
    // Default order is left is biggest
    const reversed = typeof option.reversed === "boolean" ? option.reversed : false;
    const getScale = typeof option.getScaleFunc === "function" ? option.getScaleFunc : (a) => a;
    const comp = typeof option.compareFunc === "function" ? option.compareFunc : (a, b) => a - b;
    const onDotClick = typeof option.onDotClick === "function" ? option.onDotClick : null;
    const onDotEnter = typeof option.onDotEnter === "function" ? option.onDotEnter : null;
    const onDotLeave = typeof option.onDotLeave === "function" ? option.onDotLeave : null;
    const tagHeight = defaultFontSize;
    const height = option.height ? option.height : 2 * radius + tagHeight;
    const colorBatchRender = new ColorBatchRender();
    let drawLabelsSeqs = [];

    // Draw dot api can be used in user defined render function
    const drawDot = (context, x, y, isEmpty, tag = null, innerLabel, useRadius, color, innerLabelColor, emptylineColor) => {
        useRadius = useRadius ? useRadius : radius;
        color = color ? color : defaultDotColor;
        emptylineColor = emptylineColor ? emptylineColor : defaultEmptyLineColor;
        innerLabelColor = innerLabelColor ? innerLabelColor : defaultInnerLableColor;
        const fontSize = useRadius * 1.5;
        const baselineY = y + useRadius;
        if (!isEmpty) {
            // Draw the dot
            colorBatchRender.lazyCreateColorSeqs(color, (context) => {
                context.beginPath();
                drawLabelsSeqs = [];
            }, (context, color) => {
                context.fillStyle = color;
                context.fill();
                context.font = `${fontSize}px ${fontFamily}`;
                context.textBaseline = "top";
                context.textAlign = "center";
                context.fontWeight = 400;
                context.fillStyle = innerLabelColor;
                drawLabelsSeqs.forEach(seq => seq());
            });
            colorBatchRender.addSeq(color, (context, color) => {
                context.arc(x + dotMargin + useRadius, baselineY, useRadius, 0, 2 * Math.PI);
                if (typeof innerLabel === "number" || typeof innerLabel === "string") {
                    drawLabelsSeqs.push(() => {
                        // Draw the inner label
                        const innerLabelSize = context.measureText(innerLabel);
                        const fontHeight = innerLabelSize.fontBoundingBoxAscent + innerLabelSize.fontBoundingBoxDescent;
                        const actualHeight = innerLabelSize.actualBoundingBoxAscent + innerLabelSize.actualBoundingBoxDescent;
                        const realStartGap = innerLabelSize.fontBoundingBoxAscent - innerLabelSize.actualBoundingBoxAscent;
                        context.fillText(innerLabel, x + dotMargin + useRadius, y - realStartGap + useRadius - (actualHeight < fontHeight ?  actualHeight : fontHeight) / 2);
                    });
                }
            });
        } else {
            // Draw the empty
            colorBatchRender.lazyCreateColorSeqs(emptylineColor, (context) => {
                context.beginPath();
            }, (context, color) => {
                context.strokeStyle = color;
                context.stroke();
            });
            colorBatchRender.addSeq(emptylineColor, (context) => {
                context.moveTo(x + dotMargin, baselineY);
                context.lineTo(x + dotMargin + 2 * useRadius, baselineY);
                context.lineWidth = 1;
            });
        }

        // Draw the tag
        if (typeof tag === "number" || typeof tag === "string") {
            context.font = `${defaultFontSize}px ${fontFamily}`;
            context.fillStyle = color;
            context.textAlign = "center";
            context.textBaseline = "top";
            context.fillText(tag, x + dotMargin + radius, baselineY + useRadius);
        }
        
    };
    const render = typeof option.renderFactory === "function" ? option.renderFactory(drawDot) : (dot, context, x, y) => drawDot(context, x, y, !dot);
    const sortData = option.sortData === true ? option.sortData : false;

    // Initialize
    const initDots = dots.map((dot) => dot);
    const initScales = scales.map((scale) => scale);
    if (sortData) {
        initDots.sort((a, b) => comp(getScale(a), getScale(b)));
        initScales.sort(comp);
    }
    const filterDots = (dots) => dots.filter(dot => typeof dot._x === 'number');
    let inCacheDots = [];
    const getMouseEventTirggerDots = (e, scrollLeft, element) => {
        const {x, y} = getMousePosInCanvas(e, element);
        return inCacheDots.filter(dot => pointCircleCollisionDetact({x, y},
            {
                x: dot._dotCenter.x - (scrollLeft - dot._cachedScrollLeft),
                y: dot._dotCenter.y,
                radius: radius
            }));
    }

    const dotWidth = 2 * (radius + dotMargin);
    const padding = 100 * dotWidth / getDevicePixelRatio();
    const xScrollStreamRender = xScrollStreamRenderFactory(height);

    const redraw = (startX, renderWidth, element, stateDiff, state) => {
        const scrollLeft = typeof stateDiff.scrollLeft === 'number' ? stateDiff.scrollLeft : state.scrollLeft;
        const scales = stateDiff.scales ? stateDiff.scales : state.scales;
        const dots = stateDiff.dots ? stateDiff.dots : state.dots;
        // This color maybe change when switch dark/light mode
        const defaultLineColor = getComputedStyle(document.body).getPropertyValue('--borderColorInlineElement');

        const context = element.getContext("2d", { alpha: false });
        // Clear pervious batchRender
        colorBatchRender.clear();
        // Draw the time line
        colorBatchRender.lazyCreateColorSeqs(defaultLineColor, (context) => {
            context.beginPath();
        }, (context, color) => {
            context.lineWidth = 1;
            context.strokeStyle = color;
            context.stroke();
        });
        colorBatchRender.addSeq(defaultLineColor, (context) => {
            context.moveTo(startX, radius);
            context.lineTo(startX + renderWidth, radius);
        });

        // Draw the dots
        // First, Calculate the render range:
        let startScalesIndex = Math.floor((scrollLeft + startX) / dotWidth);
        if (startScalesIndex < 0)
            startScalesIndex = 0;
        let endScalesIndex = startScalesIndex + Math.ceil((renderWidth) / dotWidth);
        if (endScalesIndex >= scales.length)
            endScalesIndex = scales.length - 1;
        let currentDotIndex = startScalesIndex - (scales.length - dots.length);
        if (currentDotIndex < 0)
            currentDotIndex = 0;
        for (let i = currentDotIndex; i <= startScalesIndex; i++) {
            const compResult = comp(scales[startScalesIndex], getScale(dots[currentDotIndex]));
            if (!reversed) {
                if (compResult > 0)
                    currentDotIndex ++;
                else
                    break;
            } else {
                if (compResult < 0)
                    currentDotIndex ++;
                else
                    break;
            }
        }

        // Use this to decrease colision search scope
        inCacheDots = [];
        for (let i = startScalesIndex; i <= endScalesIndex; i++) {
            let x = i * dotWidth - scrollLeft;
            if (currentDotIndex < dots.length && comp(scales[i], getScale(dots[currentDotIndex])) === 0) {
                render(dots[currentDotIndex], context, x, 0);
                dots[currentDotIndex]._dotCenter = {x: x + dotMargin + radius, y: radius};
                dots[currentDotIndex]._cachedScrollLeft = scrollLeft;
                inCacheDots.push(dots[currentDotIndex]);
                currentDotIndex += 1;
            } else
                render(null, context, x, 0);
        }
        colorBatchRender.batchRender(context);
    };

    return ListProviderReceiver((updateContainerWidth, onContainerScroll, onResize) => {
        const mouseMove = (e) => {
            let dots = getMouseEventTirggerDots(e, canvasRef.state.scrollLeft, canvasRef.element);
            if (dots.length) {
                if (onDotEnter) {
                    dots[0].tipPoints = [
                        {x: dots[0]._dotCenter.x, y: dots[0]._dotCenter.y - 3 * radius / 2},
                        {x: dots[0]._dotCenter.x, y: dots[0]._dotCenter.y + radius / 2},
                    ];
                    onDotEnter(dots[0], e, canvasRef.element.getBoundingClientRect());
                }
                canvasRef.element.style.cursor = "pointer";
            } else {
                if (onDotLeave)
                    onDotLeave(e, canvasRef.element.getBoundingClientRect());
                canvasRef.element.style.cursor = "default";
            }
        }
        const onScrollAction = (e) => {
            canvasRef.setState({scrollLeft: e.target.scrollLeft / getDevicePixelRatio()});
            mouseMove(e);
        };
        const onResizeAction = (width) => {
            canvasRef.setState({width: width});
        };

        const canvasRef = REF.createRef({
            state: {
                dots: initDots,
                scales: initScales,
                scrollLeft: 0,
                width: 0,
                onScreen: false,
            },
            onElementMount: (element) => {
                setupCanvasHeightWithDpr(element, height);
                setupCanvasContextScale(element);
                if (onDotClick) {
                    element.addEventListener('click', (e) => {
                        let dots = getMouseEventTirggerDots(e, canvasRef.state.scrollLeft, element);
                        if (dots.length)
                            onDotClick(dots[0], e);
                    });
                }

                if (onDotClick || onDotEnter || onDotLeave)
                    element.addEventListener('mousemove', mouseMove);
                if (onDotLeave)
                    element.addEventListener('mouseleave', (e) => onDotLeave(e, element.getBoundingClientRect()));

                createInsertionObservers(element, (entries) => {
                    canvasRef.setState({onScreen: entries[0].isIntersecting});
                }, 0, 0.01, 0.01);
            },
            onElementUnmount: (element) => {
                onContainerScroll.stopAction(onScrollAction);
                onResize.stopAction(onResizeAction);
                // Clean the canvas, free its memory
                element.width = 0;
                element.height = 0;
            },
            onStateUpdate: (element, stateDiff, state) => {
                if (!state.onScreen && !stateDiff.onScreen)
                    return;
                if (stateDiff.scales || stateDiff.dots || typeof stateDiff.scrollLeft === 'number' || typeof stateDiff.width === 'number' || stateDiff.onScreen) {
                    if (stateDiff.scales)
                        stateDiff.scales = stateDiff.scales.map(x => x);
                    if (stateDiff.dots)
                        stateDiff.dots = stateDiff.dots.map(x => x);
                    xScrollStreamRender(redraw, element, stateDiff, state);
                }
            }
        });

        updateContainerWidth(scales.length * dotWidth * getDevicePixelRatio());
        const updateData = (dots, scales) => {
            updateContainerWidth(scales.length * dotWidth * getDevicePixelRatio());
            canvasRef.setState({
                dots: dots,
                scales: scales,
            });
        };
        if (typeof option.exporter === "function")
            option.exporter(updateData);
        onContainerScroll.action(onScrollAction);
        onResize.action(onResizeAction);
        return `<div class="series">
            <canvas ref="${canvasRef}" width="0" height="0">
        </div>`;
    });
}

Timeline.ExpandableSeriesComponent = (mainSeries, subSerieses, exporter) => {
    const ref = REF.createRef({
        state: {expanded: false},
        onStateUpdate: (element, stateDiff) => {
            if (stateDiff.expanded === false) {
                element.children[0].style.display = 'none';
                element.children[1].style.display = 'block';
                element.children[2].style.display = 'none';
            } else if (stateDiff.expanded === true) {
                element.children[0].style.display = 'block';
                element.children[1].style.display = 'none';
                element.children[2].style.display = 'block';
            }
        }
    });
    if (exporter)
        exporter((expanded) => ref.setState({expanded: expanded}));
    return ListProviderReceiver((updateContainerWidth, onContainerScroll, onResize) => {
        return `<div class="groupSeries" ref="${ref}">
            <div class="series" style="display:none;"></div>
            <div>${mainSeries(updateContainerWidth, onContainerScroll, onResize)}</div>
            <div style="display:none">${subSerieses.map((subSeries) => subSeries(updateContainerWidth, onContainerScroll, onResize)).join("")}</div>
        </div>`;
    });
}

Timeline.HeaderComponent = (label) => {
    return `<div class="series">${label}</div>`;
}

Timeline.ExpandableHeaderComponent = (mainLabel, subLabels, exporter) => {
    const ref = REF.createRef({
        state: {expanded: false},
        onStateUpdate: (element, stateDiff) => {
            if (stateDiff.expanded === false)
                element.children[1].style.display = "none";
            else if (stateDiff.expanded === true)
                element.children[1].style.display = "block";
        }
    });
    if (exporter)
        exporter((expanded) => ref.setState({expanded: expanded}));
    return `<div ref="${ref}">
        <div class="series">
            ${mainLabel}
        </div>
        <div style="display:none">
            ${subLabels.map(label => `<div class="series">${label}</div>`).join("")}
        </div>
    </div>`;
}

Timeline.SeriesWithHeaderComponent = (header, series) => {
    return {header, series};
}

Timeline.ExpandableSeriesWithHeaderExpanderComponent = (mainSeriesWithLable, ...subSeriesWithLable) => {
    const ref = REF.createRef({
        state: {expanded: false},
        onStateUpdate: (element, stateDiff) => {
            if (stateDiff.expanded === false)
                element.innerText = "+";
            else if (stateDiff.expanded === true)
                element.innerText = "-";
        }
    });
    const mainLabel = mainSeriesWithLable.header;
    const subLabels = subSeriesWithLable.map(item => item.header);
    const mainSeries = mainSeriesWithLable.series;
    const subSerieses = subSeriesWithLable.map(item => item.series);
    const expandedEvent = new EventStream();
    const clickEvent = ref.fromEvent('click').action(() => {
        let expanded = ref.state.expanded;
        expandedEvent.add(!expanded);
        ref.setState({expanded: !expanded});
    });
    const composer = FP.composer(FP.currying((setHeaderExpand, setSeriesExpand) => {
        expandedEvent.action((expanded) => {
            setHeaderExpand(expanded);
            setSeriesExpand(expanded);
        })
    }));
    return {
        header: Timeline.ExpandableHeaderComponent(`<a class="link-button" href="javascript:void(0)" ref="${ref}">+</a>` + mainLabel, subLabels, composer),
        series: Timeline.ExpandableSeriesComponent(mainSeries, subSerieses, composer),
    }
}

Timeline.CanvasXAxisComponent = (scales, option = {}) => {
    // Get configuration
    const getScaleKey = typeof option.getScaleFunc === "function" ? option.getScaleFunc : (a) => a;
    const comp = typeof option.compareFunc === "function" ? option.compareFunc : (a, b) => a - b;
    const onScaleClick = typeof option.onScaleClick === "function" ? option.onScaleClick : null;
    const onScaleEnter = typeof option.onScaleEnter === "function" ? option.onScaleEnter : null;
    const onScaleLeave = typeof option.onScaleLeave === "function" ? option.onScaleLeave : null;
    const sortData = option.sortData === true ? option.sortData : false;
    const getLabel = typeof option.getLabelFunc === "function" ? option.getLabelFunc : (a) => a;
    const isTop = typeof option.isTop === "boolean" ? option.isTop : false;

    // Get the css value, this component assume to use with webkit.css
    const computedStyle = getComputedStyle(document.body);
    const fontFamily = computedStyle.getPropertyValue('font-family');
    const fontSize = computedStyle.getPropertyValue('--tinySize');
    const fontSizeNumber = parseInt(fontSize);
    const fontColor = onScaleClick ? computedStyle.getPropertyValue('--linkColor') : computedStyle.getPropertyValue('color');
    const fontRotate = 60 * Math.PI / 180;
    const fontTopRotate = 300 * Math.PI / 180;
    const linkColor = computedStyle.getPropertyValue('--linkColor');
    const scaleWidth = parseInt(computedStyle.getPropertyValue('--smallSize')) + parseInt(computedStyle.getPropertyValue('--tinySize'));
    const scaleTagLineHeight = parseInt(computedStyle.getPropertyValue('--smallSize'));
    const scaleTagLinePadding = 10;
    const scaleBroadLineHeight = parseInt(computedStyle.getPropertyValue('--tinySize')) / 2;
    const maxinumTextHeight = scaleWidth * 4.5;
    const canvasHeight = typeof option.height === "number" ? option.height : parseInt(computedStyle.getPropertyValue('--smallSize')) * 5;
    const sqrt3 = Math.sqrt(3);
    const colorBatchRender = new ColorBatchRender();

    const drawScale = (scaleLabel, group, context, x, y, isHoverable, lineColor, groupColor) => {
        const computedStyle = getComputedStyle(document.body);
        const usedLineColor = lineColor ? lineColor : computedStyle.getPropertyValue('--borderColorInlineElement');
        const usedGroupColor = groupColor ? groupColor : isDarkMode() ? computedStyle.getPropertyValue('--white') : computedStyle.getPropertyValue('--black');
        const totalWidth = group * scaleWidth;
        const baseLineY = isTop ? y + canvasHeight - scaleBroadLineHeight : y + scaleBroadLineHeight;
        const middlePointX = x + totalWidth / 2;
        if (group > 1) {
            colorBatchRender.lazyCreateColorSeqs(usedGroupColor, (context) => {
                context.beginPath();
            }, (context, color) => {
                context.lineWidth = 1;
                context.strokeStyle = color;
                context.stroke();
            });
            colorBatchRender.addSeq(usedGroupColor, (context) => {
                context.moveTo(x + context.lineWidth, isTop ? canvasHeight : y);
                context.lineTo(x + context.lineWidth, baseLineY);
                context.moveTo(x, baseLineY);
                context.lineTo(x + totalWidth, baseLineY);
                context.moveTo(x + totalWidth, isTop ? canvasHeight : y);
                context.lineTo(x + totalWidth, baseLineY);
                context.moveTo(middlePointX, baseLineY);
                if (!isTop)
                    context.lineTo(middlePointX, baseLineY + scaleTagLineHeight - scaleTagLinePadding);
                else
                    context.lineTo(middlePointX, baseLineY - scaleTagLineHeight + scaleTagLinePadding);
            });
        } else {
            colorBatchRender.lazyCreateColorSeqs(usedLineColor, (context) => {
                context.beginPath();
            }, (context, color) => {
                context.lineWidth = 1;
                context.strokeStyle = color;
                context.stroke();
            });
            colorBatchRender.addSeq(usedLineColor, (context) => {
                context.moveTo(middlePointX, baseLineY);
                if (!isTop)
                    context.lineTo(middlePointX, baseLineY + scaleTagLineHeight - scaleTagLinePadding);
                else
                    context.lineTo(middlePointX, baseLineY - scaleTagLineHeight + scaleTagLinePadding);
            });
        }
        // Draw Tag
        context.font = `${fontSize} ${fontFamily}`;
        context.fillStyle = fontColor;
        context.save();
        if (!isTop) {
            context.translate(middlePointX, baseLineY + scaleTagLineHeight);
            context.rotate(fontRotate);
            context.translate(0 - middlePointX, 0 - baseLineY - scaleTagLineHeight);
            context.fillText(getLabel(scaleLabel), middlePointX, baseLineY + scaleTagLineHeight);
        } else {
            context.translate(middlePointX, baseLineY - scaleTagLineHeight);
            context.rotate(fontTopRotate);
            context.translate(0 - middlePointX, 0 - baseLineY + scaleTagLineHeight);
            context.fillText(getLabel(scaleLabel), middlePointX, baseLineY - scaleTagLineHeight);
        }
        context.restore();
    };
    const render = typeof option.renderFactory === "function" ? option.renderFactory(drawScale) : (scaleLabel, scaleGroup, context, x, y) => drawScale(scaleLabel, scaleGroup, context, x, y);

    const padding = 100 * scaleWidth / getDevicePixelRatio();
    const xScrollStreamRender = xScrollStreamRenderFactory(canvasHeight);
    let onScreenScales = [];

    const getMouseEventTirggerScales = (e, scrollLeft, element) => {
        const {x, y} = getMousePosInCanvas(e, element);
        return onScreenScales.filter(scale => {
            const labelLength = getLabel(scale.label).length;
            const width = labelLength * fontSizeNumber / 2;
            const height = labelLength * fontSizeNumber / 2 * sqrt3;
            const point1 = {
                x: scale._tagTop.x - scrollLeft - (isTop ? fontSizeNumber / 2 * sqrt3 : 0),
                y: scale._tagTop.y + (fontSizeNumber / 2 + scaleTagLineHeight) * (isTop ? -1 : 1),
            };
            const point2 = {
                x: point1.x + fontSizeNumber / 2 * sqrt3,
                y: scale._tagTop.y + scaleTagLineHeight  * (isTop ? -1 : 1)
            };
            const point3 = {
                x: point2.x + width,
                y: point2.y + height * (isTop ? -1 : 1),
            };
            const point4 = {
                x: point1.x + width,
                y: point1.y + height * (isTop ? -1 : 1),
            };
            return pointPolygonCollisionDetect({x, y}, [point1, point2, point3, point4]);
        });
    };
    const redraw = (startX, renderWidth, element, stateDiff, state) => {
        const scrollLeft = typeof stateDiff.scrollLeft === 'number' ? stateDiff.scrollLeft : state.scrollLeft;
        const scales = stateDiff.scales ? stateDiff.scales : state.scales;
        const scalesMapLinkList = stateDiff.scalesMapLinkList ? stateDiff.scalesMapLinkList : state.scalesMapLinkList;
        const width = typeof stateDiff.width === 'number' ? stateDiff.width : state.width;
        const usedLineColor = computedStyle.getPropertyValue('--borderColorInlineElement');
        const baseLineY = isTop ? canvasHeight - scaleBroadLineHeight : scaleBroadLineHeight;
        const context = element.getContext("2d", { alpha: false });
        let currentStartScaleIndex = Math.floor(scrollLeft / scaleWidth);
        if (currentStartScaleIndex < 0)
            currentStartScaleIndex = 0;
        const currentStartScaleKey = getScaleKey(scales[currentStartScaleIndex]);
        let currentEndScaleIndex = Math.ceil((scrollLeft + renderWidth) / scaleWidth);
        currentEndScaleIndex = currentEndScaleIndex >= scales.length ? scales.length - 1 : currentEndScaleIndex;
        const currentEndScaleKey = getScaleKey(scales[currentEndScaleIndex]);
        const currentStartNode = scalesMapLinkList.map.get(currentStartScaleKey);
        const currentEndNode = scalesMapLinkList.map.get(currentEndScaleKey);
        if (!currentEndNode) {
            console.error(currentEndScaleKey);
        }
        let now = currentStartNode;
        // Clear pervious batch render
        colorBatchRender.clear();
        colorBatchRender.lazyCreateColorSeqs(usedLineColor, (context) => {
            context.beginPath();
        }, (context, color) => {
            context.lineWidth = 1;
            context.strokeStyle = color;
            context.stroke();
        });
        colorBatchRender.addSeq(usedLineColor, (context) => {
            context.moveTo(0, baseLineY);
            context.lineTo(element.logicWidth, baseLineY);
        });

        onScreenScales = [];
        while (now != currentEndNode.next) {
            const label = now.label;
            const group = now.group;
            render(label, group, context, now.x - scrollLeft, 0);
            now._tagTop = {x: now.x + group * scaleWidth / 2, y: isTop ? canvasHeight - scaleBroadLineHeight : scaleBroadLineHeight};
            onScreenScales.push(now);
            now = now.next;
        }
        colorBatchRender.batchRender(context);
    };

    // Initialize
    // Do a copy, sorting will not have side effect
    const initScales = scales.map((item) => item);
    if (sortData)
        initScales.sort(comp);

    const getScalesMapLinkList = (scales) => {
        const res = {
            map: new Map(),
            linkListHead: {next: null, group: null}
        };
        let now = res.linkListHead;
        let currentX = 0;
        scales.forEach((scale) => {
            let key = getScaleKey(scale);
            if (res.map.has(key))
                res.map.get(key).group += 1;
            else {
                now.next = {next: null, group: 1, label: scale, x: currentX};
                now = now.next;
                res.map.set(key, now);
            }
            currentX += scaleWidth;
        });
        return res;
    };
    const initScaleGroupMapLinkList = getScalesMapLinkList(initScales);


    return {
        series: ListProviderReceiver((updateContainerWidth, onContainerScroll, onResize) => {
            const mouseMove = (e) => {
                let scales = getMouseEventTirggerScales(e, canvasRef.state.scrollLeft, canvasRef.element);
                if (scales.length) {
                    if (onScaleEnter) {
                        const labelLength = getLabel(scales[0].label).length;
                        scales[0].tipPoints = [{
                            x: scales[0]._tagTop.x - canvasRef.state.scrollLeft,
                            y: scales[0]._tagTop.y + scaleTagLineHeight * (isTop ? -1 : 0),
                        }, {
                            x: scales[0]._tagTop.x - canvasRef.state.scrollLeft + labelLength * fontSizeNumber / 3 - scaleTagLineHeight * (isTop ? 1 : .25),
                            y: scales[0]._tagTop.y + (labelLength * fontSizeNumber / 2 * sqrt3) * (isTop ? -1 : 1) + scaleTagLineHeight * (isTop ? 1 : 0),
                        }];
                        onScaleEnter(scales[0], e, canvasRef.element.getBoundingClientRect());
                    }
                    canvasRef.element.style.cursor = "pointer";
                } else {
                    if (onScaleEnter)
                        onScaleLeave(e, canvasRef.element.getBoundingClientRect());
                    canvasRef.element.style.cursor = "default";
                }
            }
            const onScrollAction = (e) => {
                canvasRef.setState({scrollLeft: e.target.scrollLeft / getDevicePixelRatio()});
                mouseMove(e);
            };
            const onResizeAction = (width) => {
                canvasRef.setState({width: width});
            };

            const canvasRef = REF.createRef({
                state: {
                    scrollLeft: 0,
                    width: 0,
                    scales: initScales,
                    scalesMapLinkList: initScaleGroupMapLinkList
                },
                onElementMount: (element) => {
                    setupCanvasHeightWithDpr(element, canvasHeight);
                    setupCanvasContextScale(element);
                    if (onScaleClick) {
                        element.addEventListener('click', (e) => {
                            let scales = getMouseEventTirggerScales(e, canvasRef.state.scrollLeft, element);
                            if (scales.length)
                                onScaleClick(scales[0], e);
                        });
                    }

                    if (onScaleClick || onScaleEnter || onScaleLeave)
                        element.addEventListener('mousemove', mouseMove);
                    if (onScaleLeave)
                        element.addEventListener('mouseleave', (e) => onScaleLeave(e, element.getBoundingClientRect()));
                },
                onElementUnmount: (element) => {
                    onContainerScroll.stopAction(onScrollAction);
                    onResize.stopAction(onResizeAction);
                },
                onStateUpdate: (element, stateDiff, state) => {
                    if (stateDiff.scales || typeof stateDiff.scrollLeft === 'number' || typeof stateDiff.width === 'number') {
                        xScrollStreamRender(redraw, element, stateDiff, state);
                    }
                }
            });

            updateContainerWidth(scales.length * scaleWidth * getDevicePixelRatio());
            const updateData = (scales) => {
                // In case of modification while rendering
                const scalesCopy = scales.map(x => x);
                updateContainerWidth(scalesCopy.length * scaleWidth * getDevicePixelRatio());
                canvasRef.setState({
                    scales: scalesCopy,
                    scalesMapLinkList: getScalesMapLinkList(scalesCopy)
                });
            }
            if (typeof option.exporter === "function")
                option.exporter(updateData);
            onContainerScroll.action(onScrollAction);
            onResize.action(onResizeAction);
            return `<div class="x-axis">
                <canvas ref="${canvasRef}">
            </div>`;
        }),
        isAxis: true, // Mark self as an axis,
        height: canvasHeight, // Expose Height to parent
    };
}

Timeline.CanvasContainer = (exporter, ...children) => {
    let headerAxisPlaceHolderHeight = 0;
    let topAxis = true;
    const upackChildren = (children) => {
        const headers = [];
        const serieses = [];
        children.forEach(child => {
            if (false === "series" in child) {
                console.error("Please use Timeline.SeriesWithHeaderComponent or Timeline.ExpandableSeriesWithHeaderExpanderComponent or Timeline.CanvasXAxisComponent as children");
                return;
            }
            if (child.header)
                headers.push(child.header);
            serieses.push(child.series);
            if (child.isAxis && topAxis)
                headerAxisPlaceHolderHeight += child.height;
            else if (topAxis)
                topAxis = false;
        });
        return {headers, serieses};
    };
    const {headers, serieses} = upackChildren(children);
    let composer = FP.composer(FP.currying((updateHeaders, updateSerieses) => {
        if (exporter)
            exporter((newChildren) => {
                const {headers, serieses} = upackChildren(newChildren);
                updateHeaders(headers);
                updateSerieses(serieses);
            });
    }));
    return (
        `<div class="timeline">
            <div class="header" style="padding-top:${headerAxisPlaceHolderHeight}px">
                ${ListComponent(composer, ...headers)}
            </div>
            ${XScrollableCanvasProvider(composer, ...serieses)}
        </div>`
    );
}

export {
    Timeline
};
