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

import {isDarkMode} from '../Utils.js';
import {ListComponent, ListProvider, ListProviderReceiver} from './BaseComponents.js'

function pointCircleCollisionDetact(point, circle) {
    return Math.pow(point.x - circle.x, 2) + Math.pow(point.y - circle.y, 2) <= circle.radius * circle.radius;
}

function pointRectCollisionDetect(point, rect) {
    const diffX = point.x - rect.topLeftX;
    const diffY = point.y - rect.topLeftY;
    return diffX <= rect.width && diffY <= rect.height && diffX >= 0 && diffY >= 0;
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
            if (typeof stateDiff.scrollLeft === 'number')
                element.style.transform = `translate(${stateDiff.scrollLeft}px, 0)`;
            if (stateDiff.resize) {
                element.style.width = `${element.parentElement.parentElement.offsetWidth}px`;
                resizeEventStream.add(element.offsetWidth);
            }
        }
    });
    scrollEventStream.action((e) => {
        // Provide the logic scrollLeft
        presenterRef.setState({scrollLeft: e.target.scrollLeft});
    });
    // Provide parent functions/event to children to use

    return `<div class="content" ref="${scrollRef}">
        <div ref="${containerRef}">
            <div ref="${presenterRef}">${
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

function offscreenCachedRenderFactory(padding, height) {
    let cachedScrollLeft = 0;
    let offscreenCanvas = document.createElement('canvas');

    // This function will call redrawCache to render a offscreen cache
    // and copy the viewport area from of it
    // It will trigger redrawCache when cache don't have enough space
    return (redrawCache, element, stateDiff, state) => {
        const width = typeof stateDiff.width === 'number' ? stateDiff.width : state.width;
        if (width <= 0)
            // Nothing to render
            return;

        const totalWidth = width + 2 * padding;
        const scrollLeft = typeof stateDiff.scrollLeft === 'number' ? stateDiff.scrollLeft : state.scrollLeft;
        const context = element.getContext('2d');
        let cachePosLeft = scrollLeft - cachedScrollLeft;

        if (element.logicWidth != width) {
            // Setup the dpr in case of blur
            setupCanvasWidthWithDpr(element, width);

            // We draw everything on cache
            redrawCache(offscreenCanvas, element, stateDiff, state, () => {
                cachedScrollLeft = scrollLeft < padding ? scrollLeft : scrollLeft - padding;
                cachePosLeft = scrollLeft - cachedScrollLeft;
                if (cachePosLeft < 0)
                    cachePosLeft = 0;
                context.clearRect(0, 0, element.width, element.height);
                context.drawImage(offscreenCanvas, cachePosLeft * getDevicePixelRatio(), 0,    element.width, element.height, 0, 0, width * getDevicePixelRatio(), element.height);
            });

        } else if (cachePosLeft < 0 || cachePosLeft + width > totalWidth) {
            if (scrollLeft < 0 )
                return;
            redrawCache(offscreenCanvas, element, stateDiff, state,    () => {
                cachedScrollLeft = scrollLeft < padding ? scrollLeft : scrollLeft - padding;
                cachePosLeft = scrollLeft - cachedScrollLeft;
                if (cachePosLeft < 0)
                    cachePosLeft = 0;
                context.clearRect(0, 0, element.width, element.height);
                context.drawImage(offscreenCanvas, cachePosLeft * getDevicePixelRatio(), 0,    element.width, element.height, 0, 0, width * getDevicePixelRatio(), element.height);
            });
        } else {
            if (cachePosLeft < 0)
                cachePosLeft = 0;
            context.clearRect(0, 0, element.width, element.height);
            context.drawImage(offscreenCanvas, cachePosLeft * getDevicePixelRatio(), 0,    element.width, element.height, 0, 0, width * getDevicePixelRatio(), element.height);
        }
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
    let defaultFontSize = parseInt(computedStyle.getPropertyValue('--tinySize'));

    // Get configuration
    // Default order is left is biggest
    const reversed = typeof option.reversed === "boolean" ? option.reversed : false;
    const getScale = typeof option.getScaleFunc === "function" ? option.getScaleFunc : (a) => a;
    const comp = typeof option.compareFunc === "function" ? option.compareFunc : (a, b) => a - b;
    const onDotClick = typeof option.onDotClick === "function" ? option.onDotClick : null;
    const onDotHover = typeof option.onDotHover === "function" ? option.onDotHover : null;
    const tagHeight = defaultFontSize;
    const height = option.height ? option.height : 2 * radius + tagHeight;


    // Draw dot api can be used in user defined render function
    const drawDot = (context, x, y, isEmpty, tag = null, useRadius, color, emptylineColor) => {
        useRadius = useRadius ? useRadius : radius;
        color = color ? color : defaultDotColor;
        emptylineColor = emptylineColor ? emptylineColor : defaultEmptyLineColor;
            if (!isEmpty) {
                //Draw the dot
                context.beginPath();
                context.arc(x + dotMargin + radius, y, radius, 0, 2 * Math.PI);
                context.fillStyle = color;
                context.fill();
                if (typeof tag === "number" || typeof tag === "string") {
                    context.font = `${fontFamily} ${defaultFontSize}px`;
                    const tagSize = context.measureText(tag);
                    context.fillText(tag, x + dotMargin + radius - tagSize.width / 2, radius * 2 + tagSize.emHeightAscent);
                }
            } else {
                context.beginPath();
                context.moveTo(x + dotMargin, y);
                context.lineTo(x + dotMargin + 2 * radius, y);
                context.lineWidth = 1;
                context.strokeStyle = defaultEmptyLineColor;
                context.stroke();
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
    const offscreenCachedRender = offscreenCachedRenderFactory(padding, height);

    // Generate the dot cache
    const redrawCache = (offscreenCanvas, element, stateDiff, state, notifyToRender) => {
        const scrollLeft = typeof stateDiff.scrollLeft === 'number' ? stateDiff.scrollLeft : state.scrollLeft;
        const width = typeof stateDiff.width === 'number' ? stateDiff.width : state.width;
        const totalWidth = width + 2 * padding;
        const scales = stateDiff.scales ? stateDiff.scales : state.scales;
        const dots = stateDiff.dots ? stateDiff.dots : state.dots;
        // This color maybe change when switch dark/light mode
        const defaultLineColor = getComputedStyle(document.body).getPropertyValue('--borderColorInlineElement');
        if (offscreenCanvas.logicWidth !== totalWidth) {
            setupCanvasWidthWithDpr(offscreenCanvas, totalWidth);
            setupCanvasContextScale(offscreenCanvas);
        }
        if (offscreenCanvas.logicHeight !== element.logicHeight) {
            setupCanvasHeightWithDpr(offscreenCanvas, element.logicHeight);
            setupCanvasContextScale(offscreenCanvas);
        }

        const context = offscreenCanvas.getContext("2d");
        // Clear the cache
        context.clearRect(0, 0, offscreenCanvas.width, offscreenCanvas.height);
        // Draw the time line
        context.beginPath();
        context.moveTo(0, radius);
        context.lineWidth = 1;
        context.strokeStyle = defaultLineColor;
        context.lineTo(totalWidth, radius);
        context.stroke();

        // Draw the dots
        // First, Calculate the render range:
        let startScalesIndex = Math.floor((scrollLeft - padding) / dotWidth);
        if (startScalesIndex < 0)
            startScalesIndex = 0;
        let viewportStartScaleIndex = Math.floor((scrollLeft) / dotWidth);
        if (viewportStartScaleIndex < 0)
            viewportStartScaleIndex = 0;
        let viewportEndScaleIndex = viewportStartScaleIndex + Math.floor((width) / dotWidth);
        if (viewportEndScaleIndex >= scales.length)
            viewportEndScaleIndex = scales.length - 1;
        let endScalesIndex = startScalesIndex + Math.ceil((totalWidth) / dotWidth);
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

        // Draw the dots on cache
        for (let i = startScalesIndex; i <= endScalesIndex; i++) {
            let x = i * dotWidth - (scrollLeft < padding ? scrollLeft : scrollLeft - padding);
            if (currentDotIndex < dots.length && comp(scales[i], getScale(dots[currentDotIndex])) === 0) {
                render(dots[currentDotIndex], context, x, radius);
                dots[currentDotIndex]._dotCenter = {x: x + 3 * radius, y: radius};
                dots[currentDotIndex]._cachedScrollLeft = scrollLeft < padding ? scrollLeft : scrollLeft - padding;
                inCacheDots.push(dots[currentDotIndex]);
                currentDotIndex += 1;
            } else
                render(null, context, x, radius);
            // We already drawed viewport needed, call notifyToRender to bring them to screen now
            // this will help to elemate blink
            if (i === viewportEndScaleIndex)
                notifyToRender();
        }
    };

    const canvasRef = REF.createRef({
        state: {
            dots: initDots,
            scales: initScales,
            scrollLeft: 0,
            width: 0,
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

            if (onDotClick || onDotHover) {
                element.addEventListener('mousemove', (e) => {
                    let dots = getMouseEventTirggerDots(e, canvasRef.state.scrollLeft, element);
                    if (dots.length) {
                        if (onDotHover)
                            onDotHover(dots[0], e);
                        element.style.cursor = "pointer";
                    } else
                        element.style.cursor = "default";
                });
            }
        },
        onStateUpdate: (element, stateDiff, state) => {
            const context = element.getContext("2d");
            if (stateDiff.scales || stateDiff.dots || typeof stateDiff.scrollLeft === 'number' || typeof stateDiff.width === 'number') {
                console.assert(dots.length <= scales.length);
                requestAnimationFrame(() => offscreenCachedRender(redrawCache, element, stateDiff, state));
            }
        }
    });

    return ListProviderReceiver((updateContainerWidth, onContainerScroll, onResize) => {
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
        onContainerScroll.action((e) => {
            canvasRef.setState({scrollLeft: e.target.scrollLeft / getDevicePixelRatio()});
        });
        onResize.action((width) => {
            canvasRef.setState({width: width});
        });
        return `<div class="series">
            <canvas ref="${canvasRef}">
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
        header: Timeline.ExpandableHeaderComponent(`<a href="javascript:void(0)" ref="${ref}">+</a>` + mainLabel, subLabels, composer),
        series: Timeline.ExpandableSeriesComponent(mainSeries, subSerieses, composer),
    }
}

Timeline.CanvasXAxisComponent = (scales, option = {}) => {
    // Get configuration
    const getScaleKey = typeof option.getScaleFunc === "function" ? option.getScaleFunc : (a) => a;
    const comp = typeof option.compareFunc === "function" ? option.compareFunc : (a, b) => a - b;
    const onScaleClick = typeof option.onScaleClick === "function" ? option.onScaleClick : null;
    const onScaleHover = typeof option.onScaleHover === "function" ? option.onScaleHover : null;
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
    const canvasHeight = parseInt(computedStyle.getPropertyValue('--smallSize')) * 4;
    const sqrt3 = Math.sqrt(3);

    const drawScale = (scaleLabel, group, context, x, y, isHoverable, lineColor, groupColor) => {
        const computedStyle = getComputedStyle(document.body);
        const usedLineColor = lineColor ? lineColor : computedStyle.getPropertyValue('--borderColorInlineElement');
        const usedGroupColor = groupColor ? groupColor : isDarkMode() ? computedStyle.getPropertyValue('--white') : computedStyle.getPropertyValue('--black');
        const totalWidth = group * scaleWidth;
        const baseLineY = isTop ? y + canvasHeight - scaleBroadLineHeight : y + scaleBroadLineHeight;
        if (group > 1) {
            // Draw group label
            context.beginPath();
            context.lineWidth = 1;
            context.strokeStyle = usedGroupColor;
            context.moveTo(x + context.lineWidth, isTop ? canvasHeight : y);
            context.lineTo(x + context.lineWidth, baseLineY);
            context.stroke();
        }

        // Draw tag line
        const middlePointX = x + totalWidth / 2;
        context.beginPath();
        context.moveTo(middlePointX, baseLineY);
        if (!isTop)
            context.lineTo(middlePointX, baseLineY + scaleTagLineHeight - scaleTagLinePadding);
        else
            context.lineTo(middlePointX, baseLineY - scaleTagLineHeight + scaleTagLinePadding);
        if (group > 1)
            context.strokeStyle = usedGroupColor;
        else
            context.strokeStyle = usedLineColor;
        context.stroke();

        if (group > 1) {
                // Draw the group line
                context.beginPath();
                context.moveTo(x, baseLineY);
                context.lineTo(x + totalWidth, baseLineY);
                context.strokeStyle = usedGroupColor;
                context.stroke();
        }

        // Draw tag
        if (!isTop)
            context.moveTo(middlePointX, baseLineY + scaleTagLineHeight);
        else
            context.moveTo(middlePointX, baseLineY - scaleTagLineHeight);
        context.font = `${fontSize} ${fontFamily}`;
        context.fillStyle = fontColor;
        context.save();
        if (!isTop) {
            context.rotate(fontRotate);
            context.fillText(getLabel(scaleLabel), middlePointX / 2 + sqrt3 * scaleTagLineHeight / 2, baseLineY - sqrt3 * (middlePointX / 2) + scaleTagLineHeight / 2);
        } else {
            context.rotate(fontTopRotate);
            context.fillText(getLabel(scaleLabel), middlePointX / 2 - sqrt3 * scaleTagLineHeight - fontSizeNumber, baseLineY + sqrt3 * (middlePointX / 2) -    3 * scaleTagLineHeight / 2 - fontSizeNumber);
        }
        context.restore();

        if (group > 1) {
            // Draw group label
            context.beginPath();
            context.lineWidth = 1;
            context.strokeStyle = usedGroupColor;
            context.moveTo(x + totalWidth, isTop ? canvasHeight : y);
            context.lineTo(x + totalWidth, baseLineY);
            context.stroke();
        }
    };
    const render = typeof option.renderFactory === "function" ? option.renderFactory(drawScale) : (scaleLabel, scaleGroup, context, x, y) => drawScale(scaleLabel, scaleGroup, context, x, y);

    const padding = 100 * scaleWidth / getDevicePixelRatio();
    const offscreenCachedRender = offscreenCachedRenderFactory(padding, canvasHeight);
    let inCacheScales = [];

    const getMouseEventTirggerScales = (e, scrollLeft, element) => {
        const {x, y} = getMousePosInCanvas(e, element);
        return inCacheScales.filter(scale => {
            const detactBoxTopX = scale._tagTop.x - scrollLeft - fontSizeNumber * sqrt3 / 2;
            if (detactBoxTopX < 0) return false;
            const detactBoxTopY = scale._tagTop.y;
            const detactBoxWidth = fontSizeNumber * sqrt3 / 2 + scale.label.toString().length * fontSizeNumber / 2;
            const detactBoxHeight = fontSizeNumber / 2 + scale.label.toString().length * fontSizeNumber / 2 * sqrt3;
            return pointRectCollisionDetect({x, y}, {
                    topLeftX: detactBoxTopX,
                    topLeftY: isTop ? detactBoxTopY - detactBoxHeight : detactBoxTopY,
                    width: detactBoxWidth,
                    height: detactBoxHeight
                });
        });
    };
    const redrawCache = (offscreenCanvas, element, stateDiff, state, notifyToRender) => {
        const scrollLeft = typeof stateDiff.scrollLeft === 'number' ? stateDiff.scrollLeft : state.scrollLeft;
        const scales = stateDiff.scales ? stateDiff.scales : state.scales;
        const scalesMapLinkList = stateDiff.scalesMapLinkList ? stateDiff.scalesMapLinkList : state.scalesMapLinkList;
        const width = typeof stateDiff.width === 'number' ? stateDiff.width : state.width;
        const usedLineColor = computedStyle.getPropertyValue('--borderColorInlineElement');
        const totalWidth = 2 * padding + width;
        const baseLineY = isTop ? canvasHeight - scaleBroadLineHeight : scaleBroadLineHeight;
        if (offscreenCanvas.logicWidth !== totalWidth) {
            setupCanvasWidthWithDpr(offscreenCanvas, totalWidth);
            setupCanvasContextScale(offscreenCanvas);
        }
        if (offscreenCanvas.logicHeight !== element.logicHeight) {
            setupCanvasHeightWithDpr(offscreenCanvas, element.logicHeight);
            setupCanvasContextScale(offscreenCanvas);
        }
        const context = offscreenCanvas.getContext("2d");
        const realScrollLeft = scrollLeft > padding ? scrollLeft - padding : scrollLeft;
        const currentStartScaleIndex = Math.floor(realScrollLeft / scaleWidth);
        const currentStartScaleKey = getScaleKey(scales[currentStartScaleIndex]);
        let currentEndScaleIndex = Math.ceil((realScrollLeft + totalWidth) / scaleWidth);
        currentEndScaleIndex = currentEndScaleIndex > scales.length ? scales.length - 1 : currentEndScaleIndex;
        const currentEndScaleKey = getScaleKey(scales[currentEndScaleIndex]);
        let viewPortEndScaleIndex = Math.ceil((realScrollLeft + width) / scaleWidth);
        viewPortEndScaleIndex = viewPortEndScaleIndex > scales.length ? scales.length - 1 : viewPortEndScaleIndex;
        const viewPortEndScaleKey = getScaleKey(scales[viewPortEndScaleIndex]);
        const currentStartNode = scalesMapLinkList.map.get(currentStartScaleKey);
        const currentEndNode = scalesMapLinkList.map.get(currentEndScaleKey);
        const viewPortEndNode = scalesMapLinkList.map.get(viewPortEndScaleKey);
        let now = currentStartNode;
        // Clear the cache
        context.clearRect(0, 0, offscreenCanvas.width, offscreenCanvas.height);
        context.moveTo(0, baseLineY);
        context.lineWidth = 1;
        context.strokeStyle = usedLineColor;
        context.lineTo(offscreenCanvas.logicWidth, baseLineY);
        context.stroke();

        inCacheScales = [];
        while (now != currentEndNode.next) {
            const label = now.label;
            const group = now.group;
            render(label, group, context, now.x - realScrollLeft, 0);
            if (now === viewPortEndNode)
                notifyToRender();
            now._tagTop = {x: now.x + group * scaleWidth / 2, y: isTop ? canvasHeight - scaleBroadLineHeight : scaleBroadLineHeight};
            inCacheScales.push(now);
            now = now.next;
        }
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

            if (onScaleClick || onScaleHover) {
                element.addEventListener('mousemove', (e) => {
                    let scales = getMouseEventTirggerScales(e, canvasRef.state.scrollLeft, element);
                    if (scales.length) {
                        if (onScaleHover)
                            onScaleHover(scales[0], e);
                        element.style.cursor = "pointer";
                    } else {
                        element.style.cursor = "default";
                    }
                });
            }
        },
        onStateUpdate: (element, stateDiff, state) => {
            if (stateDiff.scales || typeof stateDiff.scrollLeft === 'number' || typeof stateDiff.width === 'number') {
                if (stateDiff.scales)
                    state.scalesMapLinkList = getScalesMapLinkList(stateDiff.scales);
                requestAnimationFrame(() => offscreenCachedRender(redrawCache, element, stateDiff, state));
            }
        }
    });

    return {
        series: ListProviderReceiver((updateContainerWidth, onContainerScroll, onResize) => {
            updateContainerWidth(scales.length * scaleWidth * getDevicePixelRatio());
            onContainerScroll.action((e) => {
                canvasRef.setState({scrollLeft: e.target.scrollLeft / getDevicePixelRatio()});
            });
            onResize.action((width) => {
                canvasRef.setState({width: width});
            });
            return `<div class="x-axis">
                <canvas ref="${canvasRef}">
            </div>`;
        }),
        isAxis: true // Mark self as an axis
    };
}

Timeline.CanvasContainer = (exporter, ...children) => {
    const hasTopXAxis = children[0].isAxis;
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
            <div class="header ${hasTopXAxis ? "with-top-x-axis" : ""}">
                ${ListComponent(composer, ...headers)}
            </div>
            ${XScrollableCanvasProvider(composer, ...serieses)}
        </div>`
    );
}

export {
    Timeline
};
