const lightGridColor = "rgba(0,0,0,0.2)";
const darkGridColor = "rgba(0,0,0,0.5)";
const transparentColor = "rgba(0, 0, 0, 0)";
const gridBackgroundColor = "rgba(255, 255, 255, 0.6)";

// CSS Regions highlight colors.
const highlightedRegionBackgroundColor = "rgba(127, 211, 248, 0.1)";
const regionBackgroundColor = "rgba(127, 211, 248, 0.45)";
const regionStrokeColor = "rgba(98, 207, 255, 0.85)";

// CSS Regions chain highlight colors.
const regionLinkBoxBackgroundColor = "rgba(255, 255, 255, 0.71)";
const regionLinkBoxStrokeColor = "rgba(98, 207, 255, 0.85)";
const regionChainStrokeColor = "rgba(98, 207, 255, 0.85)";

// CSS Region number style.
const regionNumberFont = "bold 40pt sans-serif";
const regionNumberFillColor = "rgba(255, 255, 255, 0.9)";
const regionNumberStrokeColor = "rgb(61, 127, 204)";

// CSS Shapes highlight colors
const shapeHighlightColor = "rgba(96, 82, 127, 0.8)";
const shapeMarginHighlightColor = "rgba(96, 82, 127, 0.6)";

function drawPausedInDebuggerMessage(message)
{
    var pausedInDebugger = document.getElementById("paused-in-debugger");
    pausedInDebugger.textContent = message;
    pausedInDebugger.style.visibility = "visible";
    document.body.classList.add("dimmed");
}

function _drawRegionNumber(quad, number)
{
    context.save();
    var midPoint = _quadMidPoint(quad);
    context.font = regionNumberFont;
    context.textAlign = "center";
    context.textBaseline = "middle";
    context.fillStyle = regionNumberFillColor;
    context.fillText(number, midPoint.x, midPoint.y);
    context.strokeWidth = 4;
    context.strokeStyle = regionNumberStrokeColor;
    context.strokeText(number, midPoint.x, midPoint.y);
    context.restore();
}

function quadToPath(quad)
{
    context.beginPath();
    context.moveTo(quad[0].x, quad[0].y);
    context.lineTo(quad[1].x, quad[1].y);
    context.lineTo(quad[2].x, quad[2].y);
    context.lineTo(quad[3].x, quad[3].y);
    context.closePath();
    return context;
}

function drawOutlinedQuad(quad, fillColor, outlineColor)
{
    context.save();
    context.lineWidth = 2;
    quadToPath(quad).clip();
    context.fillStyle = fillColor;
    context.fill();
    if (outlineColor) {
        context.strokeStyle = outlineColor;
        context.stroke();
    }
    context.restore();
}

function pathCommand(context, commands, name, index, length) {
    context[name].apply(context, commands.slice(index + 1, index + length + 1));
    return index + length + 1;
}

function drawPath(context, commands, fillColor, outlineColor)
{
    context.save();
    context.beginPath();

    var commandsIndex = 0;
    var commandsLength = commands.length;
    while(commandsIndex < commandsLength) {
        switch(commands[commandsIndex]) {
        // 1 point
        case 'M':
            commandsIndex = pathCommand(context, commands, "moveTo", commandsIndex, 2);
            break;
        // 1 point
        case 'L':
            commandsIndex = pathCommand(context, commands, "lineTo", commandsIndex, 2);
            break;
        // 3 points
        case 'C':
            commandsIndex = pathCommand(context, commands, "bezierCurveTo", commandsIndex, 6);
            break;
        // 2 points
        case 'Q':
            commandsIndex = pathCommand(context, commands, "quadraticCurveTo", commandsIndex, 2);
            break;
        // 0 points
        case 'Z':
            commandsIndex = pathCommand(context, commands, "closePath", commandsIndex, 0);
            break;
        default:
            commandsIndex++;
        }
    }

    context.closePath();
    context.fillStyle = fillColor;
    context.fill();

    if (outlineColor) {
        context.lineWidth = 2;
        context.strokeStyle = outlineColor;
        context.stroke();
    }

    context.restore();
}

function drawOutlinedQuadWithClip(quad, clipQuad, fillColor)
{
    var canvas = document.getElementById("canvas");
    context.fillStyle = fillColor;
    context.save();
    context.lineWidth = 0;
    quadToPath(quad).fill();
    context.globalCompositeOperation = "destination-out";
    context.fillStyle = "red";
    quadToPath(clipQuad).fill();
    context.restore();
}

function quadEquals(quad1, quad2)
{
    return quad1[0].x === quad2[0].x && quad1[0].y === quad2[0].y &&
        quad1[1].x === quad2[1].x && quad1[1].y === quad2[1].y &&
        quad1[2].x === quad2[2].x && quad1[2].y === quad2[2].y &&
        quad1[3].x === quad2[3].x && quad1[3].y === quad2[3].y;
}

function drawGutter()
{
    var frameWidth = frameViewFullSize.width;
    var frameHeight = frameViewFullSize.height;

    if (!frameWidth || document.body.offsetWidth <= frameWidth)
        rightGutter.style.removeProperty("display");
    else {
        rightGutter.style.display = "block";
        rightGutter.style.left = frameWidth + "px";
    }

    if (!frameHeight || document.body.offsetHeight <= frameHeight)
        bottomGutter.style.removeProperty("display");
    else {
        bottomGutter.style.display = "block";
        bottomGutter.style.top = frameHeight + "px";
    }
}

function reset(resetData)
{
    var deviceScaleFactor = resetData.deviceScaleFactor;
    var viewportSize = resetData.viewportSize;
    window.frameViewFullSize = resetData.frameViewFullSize;

    window.canvas = document.getElementById("canvas");
    window.context = canvas.getContext("2d");
    window.rightGutter = document.getElementById("right-gutter");
    window.bottomGutter = document.getElementById("bottom-gutter");

    canvas.width = deviceScaleFactor * viewportSize.width;
    canvas.height = deviceScaleFactor * viewportSize.height;
    canvas.style.width = viewportSize.width + "px";
    canvas.style.height = viewportSize.height + "px";
    context.scale(deviceScaleFactor, deviceScaleFactor);

    document.getElementById("paused-in-debugger").style.visibility = "hidden";
    document.getElementById("element-title-container").innerHTML = "";
    document.body.classList.remove("dimmed");
}

function DOMBuilder(tagName, className)
{
    this.element = document.createElement(tagName);
    this.element.className = className;
}

DOMBuilder.prototype.appendTextNode = function(content)
{
    var node = document.createTextNode();
    node.textContent = content;
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

function _createElementTitle(elementInfo)
{
    var builder = new DOMBuilder("div", "element-title");
    
    builder.appendSpanIfNotNull("tag-name", elementInfo.tagName);
    builder.appendSpanIfNotNull("node-id", elementInfo.idValue, "#");
    builder.appendSpanIfNotNull("class-name", _truncateString(elementInfo.className, 50));

    builder.appendTextNode(" ");
    builder.appendSpan("node-width", elementInfo.nodeWidth);
    // \xd7 is the code for the &times; HTML entity.
    builder.appendSpan("px", "px \xd7 ");
    builder.appendSpan("node-height", elementInfo.nodeHeight);
    builder.appendSpan("px", "px");

    builder.appendPropertyIfNotNull("role-name", "Role", elementInfo.role);
    builder.appendPropertyIfNotNull("region-flow-name", "Region Flow", elementInfo.regionFlowInfo ? elementInfo.regionFlowInfo.name : null);
    builder.appendPropertyIfNotNull("content-flow-name", "Content Flow", elementInfo.contentFlowInfo ? elementInfo.contentFlowInfo.name : null);

    document.getElementById("element-title-container").appendChild(builder.element);

    return builder.element;
}

function _drawElementTitle(elementInfo, fragmentHighlight, scroll)
{
    if (!elementInfo || !fragmentHighlight.quads.length)
        return;
    
    var elementTitle = _createElementTitle(elementInfo);

    var marginQuad = fragmentHighlight.quads[0];

    var titleWidth = elementTitle.offsetWidth + 6;
    var titleHeight = elementTitle.offsetHeight + 4;

    var anchorTop = marginQuad[0].y;
    var anchorBottom = marginQuad[3].y;

    const arrowHeight = 7;
    var renderArrowUp = false;
    var renderArrowDown = false;

    var boxX = marginQuad[0].x;
    
    var containingRegion = fragmentHighlight.region;
    if (containingRegion) {
        // Restrict the position of the title box to the area of the containing region.
        var clipQuad = containingRegion.quad;
        anchorTop = Math.max(anchorTop, Math.min(clipQuad[0].y, clipQuad[1].y, clipQuad[2].y, clipQuad[3].y));
        anchorBottom = Math.min(anchorBottom, Math.max(clipQuad[0].y, clipQuad[1].y, clipQuad[2].y, clipQuad[3].y));
        boxX = Math.max(boxX, Math.min(clipQuad[0].x, clipQuad[1].x, clipQuad[2].x, clipQuad[3].x));
        boxX = Math.min(boxX, Math.max(clipQuad[0].x, clipQuad[1].x, clipQuad[2].x, clipQuad[3].x));
    }

    boxX = Math.max(2, boxX - scroll.x);
    anchorTop -= scroll.y;
    anchorBottom -= scroll.y;

    if (boxX + titleWidth > canvas.width)
        boxX = canvas.width - titleWidth - 2;

    var boxY;
    if (anchorTop > canvas.height) {
        boxY = canvas.height - titleHeight - arrowHeight;
        renderArrowDown = true;
    } else if (anchorBottom < 0) {
        boxY = arrowHeight;
        renderArrowUp = true;
    } else if (anchorBottom + titleHeight + arrowHeight < canvas.height) {
        boxY = anchorBottom + arrowHeight - 4;
        renderArrowUp = true;
    } else if (anchorTop - titleHeight - arrowHeight > 0) {
        boxY = anchorTop - titleHeight - arrowHeight + 3;
        renderArrowDown = true;
    } else
        boxY = arrowHeight;

    context.save();
    context.translate(0.5, 0.5);
    context.beginPath();
    context.moveTo(boxX, boxY);
    if (renderArrowUp) {
        context.lineTo(boxX + 2 * arrowHeight, boxY);
        context.lineTo(boxX + 3 * arrowHeight, boxY - arrowHeight);
        context.lineTo(boxX + 4 * arrowHeight, boxY);
    }
    context.lineTo(boxX + titleWidth, boxY);
    context.lineTo(boxX + titleWidth, boxY + titleHeight);
    if (renderArrowDown) {
        context.lineTo(boxX + 4 * arrowHeight, boxY + titleHeight);
        context.lineTo(boxX + 3 * arrowHeight, boxY + titleHeight + arrowHeight);
        context.lineTo(boxX + 2 * arrowHeight, boxY + titleHeight);
    }
    context.lineTo(boxX, boxY + titleHeight);
    context.closePath();
    context.fillStyle = "rgb(255, 255, 194)";
    context.fill();
    context.strokeStyle = "rgb(128, 128, 128)";
    context.stroke();

    context.restore();

    elementTitle.style.top = (boxY + 3) + "px";
    elementTitle.style.left = (boxX + 3) + "px";
}

function _quadMidPoint(quad)
{
    return {
        x: (quad[0].x + quad[1].x + quad[2].x + quad[3].x) / 4,
        y: (quad[0].y + quad[1].y + quad[2].y + quad[3].y) / 4,
    };
}

function _drawRegionLink(pointA, pointB)
{
    context.save();
    context.lineWidth = 2;
    context.strokeStyle = regionChainStrokeColor;
    context.beginPath();
    context.moveTo(pointA.x, pointA.y);
    context.lineTo(pointB.x, pointB.y);
    context.stroke();
    context.restore();
}

function _drawRegionsHighlight(regions)
{
    for (var i = 0; i < regions.length; ++i) {
        var region = regions[i];
        drawOutlinedQuad(region.borderQuad, region.isHighlighted ? highlightedRegionBackgroundColor : regionBackgroundColor, regionStrokeColor);
        _drawRegionNumber(region.borderQuad, i + 1);
    }

    for (var i = 1; i < regions.length; ++i) {
        var regionA = regions[i - 1],
            regionB = regions[i];
        _drawRegionLink(_quadMidPoint(regionA.outgoingQuad), _quadMidPoint(regionB.incomingQuad));
    }

    for (var i = 0; i < regions.length; ++i) {
        var region = regions[i];
        if (i > 0)
            drawOutlinedQuad(region.incomingQuad, regionLinkBoxBackgroundColor, regionLinkBoxStrokeColor);
        if (i !== regions.length - 1)
            drawOutlinedQuad(region.outgoingQuad, regionLinkBoxBackgroundColor, regionLinkBoxStrokeColor);
    }
}

function _drawShapeHighlight(shapeInfo) {
    if (shapeInfo.marginShape)
        drawPath(context, shapeInfo.marginShape, shapeMarginHighlightColor);

    if (shapeInfo.shape)
        drawPath(context, shapeInfo.shape, shapeHighlightColor);

    if (!(shapeInfo.shape || shapeInfo.marginShape))
        drawOutlinedQuad(shapeInfo.bounds, shapeHighlightColor, shapeHighlightColor);
}

function _drawFragmentHighlight(highlight)
{
    if (!highlight.quads.length)
        return;

    context.save();

    if (highlight.region) {
        // Clip to the containing region to avoid showing fragments that are not rendered by this region.
        quadToPath(highlight.region.quad).clip();
    }

    var quads = highlight.quads.slice();
    var contentQuad = quads.pop();
    var paddingQuad = quads.pop();
    var borderQuad = quads.pop();
    var marginQuad = quads.pop();

    var hasContent = contentQuad && highlight.contentColor !== transparentColor || highlight.contentOutlineColor !== transparentColor;
    var hasPadding = paddingQuad && highlight.paddingColor !== transparentColor;
    var hasBorder = borderQuad && highlight.borderColor !== transparentColor;
    var hasMargin = marginQuad && highlight.marginColor !== transparentColor;

    var clipQuad;
    if (hasMargin && (!hasBorder || !quadEquals(marginQuad, borderQuad))) {
        drawOutlinedQuadWithClip(marginQuad, borderQuad, highlight.marginColor);
        clipQuad = borderQuad;
    }
    if (hasBorder && (!hasPadding || !quadEquals(borderQuad, paddingQuad))) {
        drawOutlinedQuadWithClip(borderQuad, paddingQuad, highlight.borderColor);
        clipQuad = paddingQuad;
    }
    if (hasPadding && (!hasContent || !quadEquals(paddingQuad, contentQuad))) {
        drawOutlinedQuadWithClip(paddingQuad, contentQuad, highlight.paddingColor);
        clipQuad = contentQuad;
    }
    if (hasContent)
        drawOutlinedQuad(contentQuad, highlight.contentColor, highlight.contentOutlineColor);

    var width = canvas.width;
    var height = canvas.height;
    var minX = Number.MAX_VALUE, minY = Number.MAX_VALUE, maxX = Number.MIN_VALUE; maxY = Number.MIN_VALUE;
    for (var i = 0; i < highlight.quads.length; ++i) {
        var quad = highlight.quads[i];
        for (var j = 0; j < quad.length; ++j) {
            minX = Math.min(minX, quad[j].x);
            maxX = Math.max(maxX, quad[j].x);
            minY = Math.min(minY, quad[j].y);
            maxY = Math.max(maxY, quad[j].y);
        }
    }

    context.restore();
}

function showPageIndication()
{
    document.body.classList.add("indicate");
}

function hidePageIndication()
{
    document.body.classList.remove("indicate");
}

function drawNodeHighlight(highlight)
{
    context.save();
    context.translate(-highlight.scroll.x, -highlight.scroll.y);

    for (var i = 0; i < highlight.fragments.length; ++i)
        _drawFragmentHighlight(highlight.fragments[i]);

    if (highlight.elementInfo && highlight.elementInfo.regionFlowInfo)
        _drawRegionsHighlight(highlight.elementInfo.regionFlowInfo.regions);

    if (highlight.elementInfo && highlight.elementInfo.shapeOutsideInfo)
        _drawShapeHighlight(highlight.elementInfo.shapeOutsideInfo);

    context.restore();

    var elementTitleContainer = document.getElementById("element-title-container");
    elementTitleContainer.innerHTML = "";
    for (var i = 0; i < highlight.fragments.length; ++i)
        _drawElementTitle(highlight.elementInfo, highlight.fragments[i], highlight.scroll);
}

function drawQuadHighlight(highlight)
{
    context.save();
    drawOutlinedQuad(highlight.quads[0], highlight.contentColor, highlight.contentOutlineColor);
    context.restore();
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
