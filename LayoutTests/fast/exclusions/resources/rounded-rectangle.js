if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function defined(value) { return typeof value !== "undefined"; }

function convertToRoundedRect(dimensions) {
    if (defined(dimensions.shapeCenterX) && defined(dimensions.shapeCenterY)) {
        if (defined(dimensions.shapeRadius)) {
            // Convert a circle.
            dimensions.shapeX = dimensions.shapeCenterX - dimensions.shapeRadius;
            dimensions.shapeY = dimensions.shapeCenterY - dimensions.shapeRadius;
            dimensions.shapeWidth = dimensions.shapeRadius * 2;
            dimensions.shapeHeight = dimensions.shapeRadius * 2;
            dimensions.shapeRadiusX = dimensions.shapeRadius;
            dimensions.shapeRadiusY = dimensions.shapeRadius;
        } else {
            // Convert an ellipse.
            dimensions.shapeX = dimensions.shapeCenterX - dimensions.shapeRadiusX;
            dimensions.shapeY = dimensions.shapeCenterY - dimensions.shapeRadiusY;
            dimensions.shapeWidth = dimensions.shapeRadiusX * 2;
            dimensions.shapeHeight = dimensions.shapeRadiusY * 2;
        }
    }
    // Otherwise, we have a rounded rect, so no conversion is needed.
}

function xFromEllipseCenter(yFromEllipseCenter, radiusX, radiusY) {
    return radiusX * Math.sqrt(1 - Math.pow(yFromEllipseCenter / radiusY, 2));
}

function xInset(dimensions, lineTop, lineBottom) {
    var left;
    if (lineTop < dimensions.shapeHeight && lineBottom > dimensions.shapeHeight)
        lineBottom = dimensions.shapeHeight;
    if (lineTop >= dimensions.shapeHeight || (lineTop >= dimensions.shapeRadiusY && lineBottom <= dimensions.shapeHeight - dimensions.shapeRadiusY)) {
        left = 0;
    } else {
        var yFromEllipseCenter = Math.max((dimensions.shapeRadiusY - lineTop), lineBottom - (dimensions.shapeHeight - dimensions.shapeRadiusY));
        left = dimensions.shapeRadiusX - xFromEllipseCenter(yFromEllipseCenter, dimensions.shapeRadiusX, dimensions.shapeRadiusY);
    }
    return left;
}

function xOutset(dimensions, lineTop, lineBottom) {
    var left = 0;
    if (lineBottom > dimensions.shapeHeight)
        lineBottom = dimensions.shapeHeight;
    if (lineTop < dimensions.shapeHeight && (lineTop < dimensions.shapeRadiusY || lineBottom > dimensions.shapeHeight - dimensions.shapeRadiusY)) {
        var yFromEllipseCenter;
        if (lineBottom < dimensions.shapeRadiusY) {
            yFromEllipseCenter = lineBottom - dimensions.shapeRadiusY;
            left = dimensions.shapeRadiusX - xFromEllipseCenter(yFromEllipseCenter, dimensions.shapeRadiusX, dimensions.shapeRadiusY);
        } else if (lineTop > dimensions.shapeHeight - dimensions.shapeRadiusY) {
            yFromEllipseCenter = lineTop - (dimensions.shapeHeight - dimensions.shapeRadiusY);
            left = dimensions.shapeRadiusX - xFromEllipseCenter(yFromEllipseCenter, dimensions.shapeRadiusX, dimensions.shapeRadiusY);
        }
    }
    return left;
}

function generateString(dimensions, lineHeight) {
    var resultLength = 0;
    if (dimensions.shapeRadiusX == 0 || dimensions.shapeRadiusY == 0)
        resultLength = dimensions.shapeWidth * dimensions.shapeHeight / (lineHeight * lineHeight);
    else {
        for (var lineTop = 0; lineTop < dimensions.shapeHeight; lineTop += lineHeight) {
            var width = dimensions.shapeWidth - 2 * xInset(dimensions, lineTop, lineTop + lineHeight);
            resultLength += width / lineHeight;
        }
    }
    return (new Array(Math.ceil(resultLength / 2)).join("x "));
}

function simulateShape(elementId, stylesheet, dimensions, lineHeight) {
    var simpleRectangle = dimensions.shapeRadiusX == 0 || dimensions.shapeRadiusY == 0;
    var floatHeight = simpleRectangle ? dimensions.shapeHeight : lineHeight;

    stylesheet.insertRule("#" + elementId + " { width: " + dimensions.width + "px; height: " + dimensions.height + "px; font: " + lineHeight + "px/1 Ahem, sans-serif; }");
    stylesheet.insertRule("#" + elementId + " .float { height: " + floatHeight + "px; }");
    stylesheet.insertRule("#" + elementId + " .left { float: left; clear: left; }");
    stylesheet.insertRule("#" + elementId + " .right { float: right; clear: right; }");
    stylesheet.insertRule("#" + elementId + " p { -webkit-margin-before: 0; word-wrap: break-word; letter-spacing: 0; }");

    var element = document.getElementById(elementId);
    if (dimensions.shapeY > 0) {
        var paddingTop = document.createElement("div");
        paddingTop.setAttribute("class", "float left");
        paddingTop.style.width = dimensions.width + "px";
        paddingTop.style.height = dimensions.shapeY + "px";
        element.appendChild(paddingTop);
    }

    for (var y = 0; y < dimensions.shapeHeight; y+= floatHeight) {
        var inset = simpleRectangle ? 0 : xInset(dimensions, y, y + lineHeight);

        var paddingLeft = document.createElement("div");
        paddingLeft.setAttribute("class", "float left");
        paddingLeft.style.width = SubPixelLayout.roundLineLeft(dimensions.shapeX + inset) + "px";
        element.appendChild(paddingLeft);

        var paddingRight = document.createElement("div");
        paddingRight.setAttribute("class", "float right");
        paddingRight.style.width = SubPixelLayout.roundLineRight((dimensions.width - dimensions.shapeWidth - dimensions.shapeX) + inset) + "px";
        element.appendChild(paddingRight);
    }
}

function simulateShapeOutline(elementId, stylesheet, dimensions) {
    stylesheet.insertRule("#" + elementId + "{ position: relative; }");
    stylesheet.insertRule("#" + elementId + "::before { "
                            + "content: ' '; "
                            + "display: block; "
                            + "position: absolute; "
                            + "top: " + (dimensions.shapeY - 1) + "px; "
                            + "left: " + (dimensions.shapeX - 1) + "px; "
                            + "width: " + dimensions.shapeWidth + "px; "
                            + "height: " + dimensions.shapeHeight + "px; "
                            + "border: 1px solid blue; "
                            + "border-radius: " + dimensions.shapeRadiusX + "px / " + dimensions.shapeRadiusY + "px; }");
}

function generateSimulatedShapeElement(elementId, stylesheet, dimensions, lineHeight) {
    simulateShape(elementId, stylesheet, dimensions, lineHeight);
    simulateShapeOutline(elementId, stylesheet, dimensions);
    var text = document.createTextNode(generateString(dimensions, lineHeight));
    var p = document.createElement("p");
    p.appendChild(text);

    var element = document.getElementById(elementId);
    element.appendChild(p);
}

function generateShapeElement(elementId, stylesheet, dimensions, lineHeight) {
    stylesheet.insertRule("#" + elementId + " { "
        + "-webkit-shape-inside: rectangle("
        + dimensions.shapeX + "px, "
        + dimensions.shapeY + "px, "
        + dimensions.shapeWidth + "px, "
        + dimensions.shapeHeight + "px, "
        + dimensions.shapeRadiusX + "px, "
        + dimensions.shapeRadiusY + "px); "
        + "width: " + dimensions.width + "px; "
        + "height: " + dimensions.height + "px; "
        + "font: " + lineHeight + "px/1 Ahem, sans-serif; "
        + "word-wrap: break-word; }");
    stylesheet.insertRule("#" + elementId + " p { -webkit-margin-before: 0; word-wrap: break-word; letter-spacing: 0; }");
    simulateShapeOutline(elementId, stylesheet, dimensions);
    var text = document.createTextNode(generateString(dimensions, lineHeight));
    var p = document.createElement("p");
    p.appendChild(text);

    var element = document.getElementById(elementId);
    element.appendChild(p);
}

function generateShapeOutsideOnFloat(elementId, stylesheet, dimensions, floatValue, lineHeight) {
    if (defined(dimensions.shapeCenterX) && defined(dimensions.shapeCenterY)) { // circle or ellipse
        if (defined(dimensions.shapeRadius)) {
            stylesheet.insertRule("#" + elementId + " { "
                + "-webkit-shape-outside: circle("
                + dimensions.shapeCenterX + "px, "
                + dimensions.shapeCenterY + "px, "
                + dimensions.shapeRadius + "px); "
                + "float: " + floatValue + "; }");
        } else {
            stylesheet.insertRule("#" + elementId + " { "
                + "-webkit-shape-outside: ellipse("
                + dimensions.shapeCenterX + "px, "
                + dimensions.shapeCenterY + "px, "
                + dimensions.shapeRadiusX + "px, "
                + dimensions.shapeRadiusY + "px); "
                + "float: " + floatValue + "; }");
        }
    } else {
        stylesheet.insertRule("#" + elementId + " { "
            + "-webkit-shape-outside: rectangle("
            + dimensions.shapeX + "px, "
            + dimensions.shapeY + "px, "
            + dimensions.shapeWidth + "px, "
            + dimensions.shapeHeight + "px, "
            + dimensions.shapeRadiusX + "px, "
            + dimensions.shapeRadiusY + "px); "
            + "float: " + floatValue + "; }");
    }
    convertToRoundedRect(dimensions);
    simulateShapeOutline(elementId, stylesheet, dimensions);
}

// Note that this does not attempt to simulate where the float content would be
// if the shape's X and Y are not 0.
function generateSimulatedShapeOutsideOnFloat(elementId, stylesheet, dimensions, floatValue, lineHeight) {
    convertToRoundedRect(dimensions);

    var simpleRectangle = dimensions.shapeRadiusX == 0 || dimensions.shapeRadiusY == 0;
    var floatHeight = simpleRectangle ? dimensions.shapeHeight : lineHeight;

    stylesheet.insertRule("#" + elementId + " { float: " + floatValue + " }");
    stylesheet.insertRule("." + elementId + "-float { "
            + "height: " + floatHeight + "px; " 
            + "float: " + floatValue + ";"
            + "clear: " + floatValue + "; }");

    var element = document.getElementById(elementId);
    var simulationHTML = "";

    for (var y = 0; y < dimensions.shapeHeight; y+= floatHeight) {
        var outset = simpleRectangle ? 0 : xOutset(dimensions, y, y + lineHeight);

        var width = dimensions.shapeWidth - outset;

        simulationHTML += '<div class="' + elementId + '-float" style="width:' + width + 'px"></div>\n';
    }

    element.insertAdjacentHTML('afterend', simulationHTML);
    // For simulating, we ignore the x and y values, since we're not attempting
    // to draw the float's content in the right place, we just want to simulate
    // the shape's effect.
    if (floatValue == "right")
        dimensions.shapeX = -dimensions.shapeWidth;
    else 
        dimensions.shapeX = 0;
    dimensions.shapeY = 0;
    simulateShapeOutline(elementId, stylesheet, dimensions);
}
