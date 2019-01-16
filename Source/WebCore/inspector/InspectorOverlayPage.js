const boundsColor = "rgba(255,0,0,0.6)";
const lightGridColor = "rgba(0,0,0,0.2)";
const darkGridColor = "rgba(0,0,0,0.5)";
const transparentColor = "rgba(0, 0, 0, 0)";
const gridBackgroundColor = "rgba(255, 255, 255, 0.6)";

// CSS Shapes highlight colors
const shapeHighlightColor = "rgba(96, 82, 127, 0.8)";
const shapeMarginHighlightColor = "rgba(96, 82, 127, 0.6)";

const paintRectFillColor = "rgba(255, 0, 0, 0.5)";

const elementTitleFillColor = "rgb(255, 255, 194)";
const elementTitleStrokeColor = "rgb(128, 128, 128)";

let DATA = {};

class Bounds {
    constructor()
    {
        this._minX = Number.MAX_VALUE;
        this._minY = Number.MAX_VALUE;
        this._maxX = Number.MIN_VALUE;
        this._maxY = Number.MIN_VALUE;

        this._offsetX = 0;
        this._offsetY = 0;
    }

    // Public

    get minX() { return this._minX + this._offsetX; }
    get minY() { return this._minY + this._offsetY; }
    get maxX() { return this._maxX + this._offsetX; }
    get maxY() { return this._maxY + this._offsetY; }

    update(x, y)
    {
        this._minX = Math.min(this._minX, x);
        this._minY = Math.min(this._minY, y);
        this._maxX = Math.max(this._maxX, x);
        this._maxY = Math.max(this._maxY, y);
    }

    offset(x, y)
    {
        this._offsetX = x || 0;
        this._offsetY = y || 0;
    }
}

function drawPausedInDebuggerMessage(message)
{
    var pausedInDebugger = document.getElementById("paused-in-debugger");
    pausedInDebugger.textContent = message;
    pausedInDebugger.style.visibility = "visible";
    document.body.classList.add("dimmed");
}

function drawNodeHighlight(allHighlights)
{
    for (let highlight of allHighlights) {
        let bounds = new Bounds;
        bounds.offset(-highlight.scrollOffset.x, -highlight.scrollOffset.y);

        _isolateActions(() => {
            context.translate(-highlight.scrollOffset.x, -highlight.scrollOffset.y);

            for (let fragment of highlight.fragments)
                _drawFragmentHighlight(fragment, bounds);

            if (highlight.elementData && highlight.elementData.shapeOutsideData)
                _drawShapeHighlight(highlight.elementData.shapeOutsideData, bounds);
        });

        if (DATA.showRulers)
            _drawBounds(bounds);
    }

    if (allHighlights.length === 1) {
        for (let fragment of allHighlights[0].fragments)
            _drawElementTitle(allHighlights[0].elementData, fragment, allHighlights[0].scrollOffset);
    }
}

function drawQuadHighlight(highlight)
{
    let bounds = new Bounds;

    _isolateActions(() => {
        _drawOutlinedQuad(highlight.quads[0], highlight.contentColor, highlight.contentOutlineColor, bounds);
    });

    if (DATA.showRulers)
        _drawBounds(bounds);
}

function quadEquals(quad1, quad2)
{
    return quad1[0].x === quad2[0].x && quad1[0].y === quad2[0].y &&
        quad1[1].x === quad2[1].x && quad1[1].y === quad2[1].y &&
        quad1[2].x === quad2[2].x && quad1[2].y === quad2[2].y &&
        quad1[3].x === quad2[3].x && quad1[3].y === quad2[3].y;
}

function updatePaintRects(paintRectList)
{
    var context = paintRectsCanvas.getContext("2d");
    context.save();
    context.scale(window.devicePixelRatio, window.devicePixelRatio);

    context.clearRect(0, 0, paintRectsCanvas.width, paintRectsCanvas.height);

    context.fillStyle = paintRectFillColor;

    for (var rectObject of paintRectList)
        context.fillRect(rectObject.x, rectObject.y, rectObject.width, rectObject.height);

    context.restore();
}

function drawRulers()
{
    const gridLabelSize = 13;
    const gridSize = 15;
    const gridStepIncrement = 50;
    const gridStepLength = 8;
    const gridSubStepIncrement = 5;
    const gridSubStepLength = 5;


    let pageFactor = DATA.pageZoomFactor * DATA.pageScaleFactor;
    let scrollX = DATA.scrollOffset.x * DATA.pageScaleFactor;
    let scrollY = DATA.scrollOffset.y * DATA.pageScaleFactor;

    function zoom(value) {
        return value * pageFactor;
    }

    function unzoom(value) {
        return value / pageFactor;
    }

    function multipleBelow(value, step) {
        return value - (value % step);
    }

    let width = DATA.viewportSize.width / pageFactor;
    let height = DATA.viewportSize.height / pageFactor;
    let minX = unzoom(scrollX);
    let minY = unzoom(scrollY);
    let maxX = minX + width;
    let maxY = minY + height;

    // Draw backgrounds
    _isolateActions(() => {
        let offsetX = DATA.contentInset.width + gridSize;
        let offsetY = DATA.contentInset.height + gridSize;

        context.fillStyle = gridBackgroundColor;
        context.fillRect(DATA.contentInset.width, DATA.contentInset.height, gridSize, gridSize);
        context.fillRect(offsetX, DATA.contentInset.height, zoom(width) - offsetX, gridSize);
        context.fillRect(DATA.contentInset.width, offsetY, gridSize, zoom(height) - offsetY);
    });

    // Ruler styles
    _isolateActions(() => {
        context.lineWidth = 1;
        context.fillStyle = darkGridColor;

        // Draw labels
        _isolateActions(() => {
            context.translate(DATA.contentInset.width - scrollX, DATA.contentInset.height - scrollY);

            for (let x = multipleBelow(minX, gridStepIncrement * 2); x < maxX; x += gridStepIncrement * 2) {
                if (!x && !scrollX)
                    continue;

                _isolateActions(() => {
                    context.translate(zoom(x) + 0.5, scrollY);
                    context.fillText(x, 2, gridLabelSize);
                });
            }

            for (let y = multipleBelow(minY, gridStepIncrement * 2); y < maxY; y += gridStepIncrement * 2) {
                if (!y && !scrollY)
                    continue;

                _isolateActions(() => {
                    context.translate(scrollX, zoom(y) + 0.5);
                    context.rotate(-Math.PI / 2);
                    context.fillText(y, 2, gridLabelSize);
                });
            }
        });

        // Draw horizontal grid
        _isolateActions(() => {
            context.translate(DATA.contentInset.width - scrollX + 0.5, DATA.contentInset.height - scrollY);

            for (let x = multipleBelow(minX, gridSubStepIncrement); x < maxX; x += gridSubStepIncrement) {
                if (!x && !scrollX)
                    continue;

                context.beginPath();
                context.moveTo(zoom(x), scrollY);

                if (x % gridStepIncrement) {
                    context.strokeStyle = lightGridColor;
                    context.lineTo(zoom(x), scrollY + gridSubStepLength);
                } else {
                    context.strokeStyle = darkGridColor;
                    context.lineTo(zoom(x), scrollY + ((x % (gridStepIncrement * 2)) ? gridSubStepLength : gridStepLength));
                }

                context.stroke();
            }
        });

        // Draw vertical grid
        _isolateActions(() => {
            context.translate(DATA.contentInset.width - scrollX, DATA.contentInset.height - scrollY + 0.5);

            for (let y = multipleBelow(minY, gridSubStepIncrement); y < maxY; y += gridSubStepIncrement) {
                if (!y && !scrollY)
                    continue;

                context.beginPath();
                context.moveTo(scrollX, zoom(y));

                if (y % gridStepIncrement) {
                    context.strokeStyle = lightGridColor;
                    context.lineTo(scrollX + gridSubStepLength, zoom(y));
                } else {
                    context.strokeStyle = darkGridColor;
                    context.lineTo(scrollX + ((y % (gridStepIncrement * 2)) ? gridSubStepLength : gridStepLength), zoom(y));
                }

                context.stroke();
            }
        });
    });
}

function reset(payload)
{
    DATA.viewportSize = payload.viewportSize;
    DATA.deviceScaleFactor = payload.deviceScaleFactor;
    DATA.pageScaleFactor = payload.pageScaleFactor;
    DATA.pageZoomFactor = payload.pageZoomFactor;
    DATA.scrollOffset = payload.scrollOffset;
    DATA.contentInset = payload.contentInset;
    DATA.showRulers = payload.showRulers;

    window.canvas = document.getElementById("canvas");
    window.paintRectsCanvas = document.getElementById("paintrects-canvas");

    window.context = canvas.getContext("2d");

    canvas.width = DATA.deviceScaleFactor * DATA.viewportSize.width;
    canvas.height = DATA.deviceScaleFactor * DATA.viewportSize.height;
    canvas.style.width = DATA.viewportSize.width + "px";
    canvas.style.height = DATA.viewportSize.height + "px";
    context.scale(DATA.deviceScaleFactor, DATA.deviceScaleFactor);
    context.clearRect(0, 0, canvas.width, canvas.height);

    // We avoid getting the context for the paint rects canvas until we need to paint, to avoid backing store allocation.
    paintRectsCanvas.width = DATA.deviceScaleFactor * DATA.viewportSize.width;
    paintRectsCanvas.height = DATA.deviceScaleFactor * DATA.viewportSize.height;
    paintRectsCanvas.style.width = DATA.viewportSize.width + "px";
    paintRectsCanvas.style.height = DATA.viewportSize.height + "px";

    document.getElementById("paused-in-debugger").style.visibility = "hidden";
    document.getElementById("element-title-container").textContent = "";
    document.body.classList.remove("dimmed");

    document.getElementById("log").style.setProperty("top", DATA.contentInset.height + "px");
}

function DOMBuilder(tagName, className)
{
    this.element = document.createElement(tagName);
    this.element.className = className;
}

DOMBuilder.prototype.appendTextNode = function(content)
{
    let node = document.createTextNode(content);
    this.element.appendChild(node);
    return node;
}

DOMBuilder.prototype.appendSpan = function(className, value)
{
    var span = document.createElement("span");
    span.className = className;
    span.textContent = value;
    this.element.appendChild(span);
    return span;
}

DOMBuilder.prototype.appendSpanIfNotNull = function(className, value, prefix)
{
    return value ? this.appendSpan(className, (prefix ? prefix : "") + value) : null;
}

DOMBuilder.prototype.appendProperty = function(className, propertyName, value)
{
    var builder = new DOMBuilder("div", className);
    builder.appendSpan("css-property", propertyName);
    builder.appendTextNode(" ");
    builder.appendSpan("value", value);
    this.element.appendChild(builder.element);
    return builder.element;
}

DOMBuilder.prototype.appendPropertyIfNotNull = function(className, propertyName, value)
{
    return value ? this.appendProperty(className, propertyName, value) : null;
}

function _truncateString(value, maxLength)
{
    return value && value.length > maxLength ? value.substring(0, 50) + "\u2026" : value;
}

function _isolateActions(func) {
    context.save();
    func();
    context.restore();
}

function _quadToPath(quad, bounds)
{
    function parseQuadPoint(point) {
        bounds.update(point.x, point.y);
        return [point.x, point.y];
    }

    context.beginPath();
    context.moveTo(...parseQuadPoint(quad[0]));
    context.lineTo(...parseQuadPoint(quad[1]));
    context.lineTo(...parseQuadPoint(quad[2]));
    context.lineTo(...parseQuadPoint(quad[3]));
    context.closePath();
    return context;
}

function _drawOutlinedQuad(quad, fillColor, outlineColor, bounds)
{
    _isolateActions(() => {
        context.lineWidth = 2;
        _quadToPath(quad, bounds);
        context.clip();
        context.fillStyle = fillColor;
        context.fill();
        if (outlineColor) {
            context.strokeStyle = outlineColor;
            context.stroke();
        }
    });
}

function _drawPath(context, commands, fillColor, bounds)
{
    let commandsIndex = 0;

    function parsePoints(count) {
        let parsed = [];
        for (let i = 0; i < count; ++i) {
            let x = commands[commandsIndex++];
            parsed.push(x);

            let y = commands[commandsIndex++];
            parsed.push(y);

            bounds.update(x, y);
        }
        return parsed;
    }

    _isolateActions(() => {
        context.beginPath();

        while (commandsIndex < commands.length) {
            switch (commands[commandsIndex++]) {
            case 'M':
                context.moveTo(...parsePoints(1));
                break;
            case 'L':
                context.lineTo(...parsePoints(1));
                break;
            case 'C':
                context.bezierCurveTo(...parsePoints(3));
                break;
            case 'Q':
                context.quadraticCurveTo(...parsePoints(2));
                break;
            case 'Z':
                context.closePath();
                break;
            }
        }

        context.closePath();
        context.fillStyle = fillColor;
        context.fill();
    });
}

function _drawOutlinedQuadWithClip(quad, clipQuad, fillColor, bounds)
{
    _isolateActions(() => {
        context.fillStyle = fillColor;
        context.lineWidth = 0;
        _quadToPath(quad, bounds);
        context.fill();

        context.globalCompositeOperation = "destination-out";
        context.fillStyle = "red";
        _quadToPath(clipQuad, bounds);
        context.fill();
    });
}

function _drawBounds(bounds)
{
    _isolateActions(() => {
        let minX = DATA.contentInset.width;
        let maxX = DATA.viewportSize.width;
        let minY = DATA.contentInset.height;
        let maxY = DATA.viewportSize.height;

        context.beginPath();

        if (bounds.minY > minY) {
            context.moveTo(bounds.minX, bounds.minY);
            context.lineTo(bounds.minX, minY);

            context.moveTo(bounds.maxX, bounds.minY);
            context.lineTo(bounds.maxX, minY);
        }

        if (bounds.maxY < maxY) {
            context.moveTo(bounds.minX, bounds.maxY);
            context.lineTo(bounds.minX, maxY);

            context.moveTo(bounds.maxX, bounds.maxY);
            context.lineTo(bounds.maxX, maxY);
        }

        if (bounds.minX > minX) {
            context.moveTo(bounds.minX, bounds.minY);
            context.lineTo(minX, bounds.minY);

            context.moveTo(bounds.minX, bounds.maxY);
            context.lineTo(minX, bounds.maxY);
        }

        if (bounds.maxX < maxX) {
            context.moveTo(bounds.maxX, bounds.minY);
            context.lineTo(maxX, bounds.minY);

            context.moveTo(bounds.maxX, bounds.maxY);
            context.lineTo(maxX, bounds.maxY);
        }

        context.lineWidth = 1;
        context.strokeStyle = boundsColor;
        context.stroke();
    });
}

function _createElementTitle(elementData)
{
    let builder = new DOMBuilder("div", "element-title");

    builder.appendSpanIfNotNull("tag-name", elementData.tagName);
    builder.appendSpanIfNotNull("node-id", CSS.escape(elementData.idValue), "#");

    let classes = elementData.classes;
    if (classes && classes.length)
        builder.appendSpan("class-name", _truncateString(classes.map((className) => "." + CSS.escape(className)).join(""), 50));

    builder.appendSpanIfNotNull("pseudo-type", elementData.pseudoElement, "::");

    builder.appendTextNode(" ");
    builder.appendSpan("node-width", elementData.size.width);
    // \xd7 is the code for the &times; HTML entity.
    builder.appendSpan("px", "px \xd7 ");
    builder.appendSpan("node-height", elementData.size.height);
    builder.appendSpan("px", "px");

    builder.appendPropertyIfNotNull("role-name", "Role", elementData.role);

    document.getElementById("element-title-container").appendChild(builder.element);

    return builder.element;
}

function _drawElementTitle(elementData, fragmentHighlight, scroll)
{
    if (!elementData || !fragmentHighlight.quads.length)
        return;

    var elementTitle = _createElementTitle(elementData);

    var marginQuad = fragmentHighlight.quads[0];

    var titleWidth = elementTitle.offsetWidth + 6;
    var titleHeight = elementTitle.offsetHeight + 4;

    var anchorTop = marginQuad[0].y;
    var anchorBottom = marginQuad[3].y;

    const arrowHeight = 7;
    var renderArrowUp = false;
    var renderArrowDown = false;

    var boxX = marginQuad[0].x;

    boxX = Math.max(2, boxX - scroll.x);
    anchorTop -= scroll.y;
    anchorBottom -= scroll.y;

    var viewportWidth = DATA.viewportSize.width;
    if (boxX + titleWidth > viewportWidth)
        boxX = viewportWidth - titleWidth - 2;

    var viewportHeight = DATA.viewportSize.height;
    var viewportTop = DATA.contentInset.height;

    var boxY;
    if (anchorTop > viewportHeight) {
        boxY = canvas.height - titleHeight - arrowHeight;
        renderArrowDown = true;
    } else if (anchorBottom < viewportTop) {
        boxY = arrowHeight;
        renderArrowUp = true;
    } else if (anchorBottom + titleHeight + arrowHeight < viewportHeight) {
        boxY = anchorBottom + arrowHeight - 4;
        renderArrowUp = true;
    } else if (anchorTop - titleHeight - arrowHeight > viewportTop) {
        boxY = anchorTop - titleHeight - arrowHeight + 3;
        renderArrowDown = true;
    } else
        boxY = arrowHeight;

    _isolateActions(() => {
        context.translate(0.5, 0.5);
        context.beginPath();
        context.moveTo(boxX, boxY);
        if (renderArrowUp) {
            context.lineTo(boxX + (2 * arrowHeight), boxY);
            context.lineTo(boxX + (3 * arrowHeight), boxY - arrowHeight);
            context.lineTo(boxX + (4 * arrowHeight), boxY);
        }
        context.lineTo(boxX + titleWidth, boxY);
        context.lineTo(boxX + titleWidth, boxY + titleHeight);
        if (renderArrowDown) {
            context.lineTo(boxX + (4 * arrowHeight), boxY + titleHeight);
            context.lineTo(boxX + (3 * arrowHeight), boxY + titleHeight + arrowHeight);
            context.lineTo(boxX + (2 * arrowHeight), boxY + titleHeight);
        }
        context.lineTo(boxX, boxY + titleHeight);
        context.closePath();
        context.fillStyle = elementTitleFillColor;
        context.fill();
        context.strokeStyle = elementTitleStrokeColor;
        context.stroke();
    });

    elementTitle.style.top = (boxY + 3) + "px";
    elementTitle.style.left = (boxX + 3) + "px";
}

function _drawShapeHighlight(shapeInfo, bounds)
{
    if (shapeInfo.marginShape)
        _drawPath(context, shapeInfo.marginShape, shapeMarginHighlightColor, bounds);

    if (shapeInfo.shape)
        _drawPath(context, shapeInfo.shape, shapeHighlightColor, bounds);

    if (!(shapeInfo.shape || shapeInfo.marginShape))
        _drawOutlinedQuad(shapeInfo.bounds, shapeHighlightColor, shapeHighlightColor, bounds);
}

function _drawFragmentHighlight(highlight, bounds)
{
    if (!highlight.quads.length)
        return;

    _isolateActions(() => {
        let quads = highlight.quads.slice();
        let contentQuad = quads.pop();
        let paddingQuad = quads.pop();
        let borderQuad = quads.pop();
        let marginQuad = quads.pop();

        let hasContent = contentQuad && highlight.contentColor !== transparentColor || highlight.contentOutlineColor !== transparentColor;
        let hasPadding = paddingQuad && highlight.paddingColor !== transparentColor;
        let hasBorder = borderQuad && highlight.borderColor !== transparentColor;
        let hasMargin = marginQuad && highlight.marginColor !== transparentColor;

        if (hasMargin && (!hasBorder || !quadEquals(marginQuad, borderQuad)))
            _drawOutlinedQuadWithClip(marginQuad, borderQuad, highlight.marginColor, bounds);

        if (hasBorder && (!hasPadding || !quadEquals(borderQuad, paddingQuad)))
            _drawOutlinedQuadWithClip(borderQuad, paddingQuad, highlight.borderColor, bounds);

        if (hasPadding && (!hasContent || !quadEquals(paddingQuad, contentQuad)))
            _drawOutlinedQuadWithClip(paddingQuad, contentQuad, highlight.paddingColor, bounds);

        if (hasContent)
            _drawOutlinedQuad(contentQuad, highlight.contentColor, highlight.contentOutlineColor, bounds);
    });
}

function showPageIndication()
{
    document.body.classList.add("indicate");
}

function hidePageIndication()
{
    document.body.classList.remove("indicate");
}

function setPlatform(platform)
{
    document.body.classList.add("platform-" + platform);
}

function dispatch(message)
{
    var functionName = message.shift();
    window[functionName].apply(null, message);
}

function log(text)
{
    var logEntry = document.createElement("div");
    logEntry.textContent = text;
    document.getElementById("log").appendChild(logEntry);
}
