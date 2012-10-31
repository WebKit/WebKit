// The functions in this file are intended to be used to test non-complex polygons
// where horizontal lines that overlap the polygon only cross the polygon twice.

function createPolygon(vertices) {
    var xCoordinates = vertices.map( function(p) { return p.x; } );
    var yCoordinates = vertices.map( function(p) { return p.y; } );
    return {
        vertices: vertices,
        minX: Math.min.apply(null, xCoordinates),
        maxX: Math.max.apply(null, xCoordinates),
        minY: Math.min.apply(null, yCoordinates),
        maxY: Math.max.apply(null, yCoordinates)
    };
}

function createRegularPolygonVertices(size, nSides)
{
    var radius = size / (2 * Math.cos(Math.PI / nSides));
    var inset = size / 2 + 20;
    var vertices = [];

    for (var i = 0; i < nSides/2; i++) {
        var a = (1 + 2*i) * (Math.PI / nSides);
        vertices[i] = {x: Math.floor(radius * Math.cos(a)), y: Math.floor(radius * Math.sin(a))};
        vertices[nSides - i - 1] = {x: vertices[i].x, y: -vertices[i].y}  // Y axis symmetry
    }

    for (var i = 0; i < nSides; i++) {
        vertices[i].x += inset;
        vertices[i].y += inset;
    }

    return vertices;
}

function subpixelRound(f) { 
    return Math.floor(!hasSubpixelSupport ? f : (Math.floor(f * 64) + 32) / 64);  // see FractionLayoutUnit::round()
}

// Return two X intercepts of the horizontal line at y. We're assuming that the polygon
// has 0 or 2 intercepts for all y. Of course this isn't true for polygons in general,
// just the ones used by the test cases supported by this file.

function polygonXIntercepts(polygon, y) {
    var vertices = polygon.vertices;
    var foundXIntercept = false;
    var interceptsMinX, interceptsMaxX;

    for(var i = 0; i < vertices.length; i++) {
        var v1 = vertices[i];
        var v2 = vertices[(i + 1) % vertices.length];

        if (Math.max(v1.y, v2.y) < y || Math.min(v1.y, v2.y) > y)
            continue;

        if (v1.y == y && v2.y == y) {  // horizontal edge 
            if (y != polygon.maxY)
                continue;

            if (!foundXIntercept) {
                interceptsMinX = Math.min(v1.x, v2.x);
                interceptsMaxX = Math.max(v1.x, v2.x);
                foundXIntercept = true;
            }
            else {
                interceptsMinX = Math.min(v1.x, v2.x, interceptsMinX);
                interceptsMaxX = Math.max(v1.x, v2.x, interceptsMaxX);
            }
        }
        else {
            var interceptX;

            if (v1.y == y) 
                interceptX = v1.x;
            else if (v2.y == y) 
                interceptX = v2.x;
            else
                interceptX = ((y - v1.y) * (v2.x - v1.x) / (v2.y - v1.y)) + v1.x;

            if (!foundXIntercept) {
                interceptsMinX = interceptsMaxX = interceptX;
                foundXIntercept = true;
            }
            else {
                interceptsMinX = Math.min(interceptX, interceptsMinX);
                interceptsMaxX = Math.max(interceptX, interceptsMaxX);
            }
        }
    }

    if (!foundXIntercept)
        return [];

    return [subpixelRound(interceptsMinX), Math.floor(interceptsMaxX)];
}

function polygonLineIntercepts(polygon, y, lineHeight) {
    var i1 = polygonXIntercepts(polygon, y);
    var i2 = polygonXIntercepts(polygon, y + lineHeight);

    if (i1.length < 2)
        return i2;
    if (i2.length < 2)
        return i1;

    return [Math.max(i1[0], i2[0]), Math.min(i1[1], i2[1])];
}

// Generate an "X X ..." string that contains enough characters to fill the polygon. Each
// character occupies a lineHeight size square cell, the top of first line of characters is aligned
// with the the polygon's bounds minimum Y value, and each line of characters fills the interior
// of the polygon.

function generatePolygonContentString(polygon, lineheight) {
    var result = "";

    for (var y = polygon.minY; y < polygon.maxY; y += lineHeight) {
        var xIntercepts = polygonLineIntercepts(polygon, y, lineHeight);
        var lengthInCells = Math.floor(xIntercepts[1] / lineHeight) - Math.ceil(xIntercepts[0] / lineHeight);
        for (var i = 0; i < lengthInCells / 2; i++)
            result += "X ";  // all lines end in a space, to enable line breaking
    }

    return result;
}

function generatePolygonShapeInsideElement(elementId, stylesheet, polygon, lineHeight) {
    var verticesString = polygon.vertices.map( function(p) { return p.x + "px " + p.y + "px"; } ).join(", ");
    stylesheet.insertRule("#" + elementId + " { "
        + "-webkit-shape-inside: polygon(" + verticesString + "); "
        + "width: " + polygon.maxX + "px; "
        + "height: " + polygon.maxY + "px; "
        + "font: " + lineHeight + "px/1 Ahem, sans-serif; }");
    stylesheet.insertRule("#" + elementId + " p { -webkit-margin-before: 0; word-wrap: break-word; letter-spacing: 0;}");

    var text = document.createTextNode(generatePolygonContentString(polygon, lineHeight));
    var p = document.createElement("p");
    p.appendChild(text);

    var element = document.getElementById(elementId);
    element.appendChild(p);
}

function generatePolygonSVGElements(elementId, stylesheet, polygon, lineHeight) {
    var svgNS = "http://www.w3.org/2000/svg";

    var svgPolygon = document.createElementNS(svgNS, "polygon");
    svgPolygon.setAttribute("points", polygon.vertices.map( function(p) { return p.x + "," + p.y; } ).join(" "));
    svgPolygon.setAttribute("fill", "#636363");

    var svgElement = document.getElementById(elementId);
    svgElement.style.width = polygon.maxX + "px";
    svgElement.style.height = polygon.maxY + "px";
    svgElement.appendChild(svgPolygon);
}

function simulatePolygonShape(elementId, stylesheet, polygon, lineHeight) {
    var width = Math.ceil(polygon.maxX);
    var height = Math.ceil(polygon.maxY);
    stylesheet.insertRule("#" + elementId + " { width: " + width + "px; height: " + height + "px; font: " + lineHeight + "px/1 Ahem, sans-serif;}");
    stylesheet.insertRule("#" + elementId + " .float { height: " + lineHeight + "px; }");
    stylesheet.insertRule("#" + elementId + " .left { float: left; clear: left; }");
    stylesheet.insertRule("#" + elementId + " .right { float: right; clear: right; }");
    stylesheet.insertRule("#" + elementId + " p { -webkit-margin-before: 0; word-wrap: break-word; letter-spacing: 0; }");

    var element = document.getElementById(elementId);

    var paddingTop = document.createElement("div");
    paddingTop.setAttribute("class", "float left");
    paddingTop.style.width = width + "px";
    paddingTop.style.height = polygon.minY + "px";
    element.appendChild(paddingTop);

    for (var y = polygon.minY; y < polygon.maxY; y += lineHeight) {
        var xIntercepts = polygonLineIntercepts(polygon, y, lineHeight);
        var left = xIntercepts[0];
        var right = xIntercepts[1];

        var paddingLeft = document.createElement("div");
        paddingLeft.setAttribute("class", "float left");
        paddingLeft.style.width = left + "px";
        element.appendChild(paddingLeft);

        var paddingRight = document.createElement("div");
        paddingRight.setAttribute("class", "float right");
        paddingRight.style.width = width - right + "px";
        element.appendChild(paddingRight);
    }
}

function generateSimulatedPolygonShapeInsideElement(elementId, stylesheet, polygon, lineHeight) {
    var width = Math.ceil(polygon.maxX);
    var height = Math.ceil(polygon.maxY);

    stylesheet.insertRule("#" + elementId + " { width: " + width + "px; height: " + height + "px; font: " + lineHeight + "px/1 Ahem, sans-serif;}");
    stylesheet.insertRule("#" + elementId + " .float { height: " + lineHeight + "px; }");
    stylesheet.insertRule("#" + elementId + " .left { float: left; clear: left; }");
    stylesheet.insertRule("#" + elementId + " .right { float: right; clear: right; }");
    stylesheet.insertRule("#" + elementId + " p { -webkit-margin-before: 0; word-wrap: break-word; letter-spacing: 0; }");

    var element = document.getElementById(elementId);

    var paddingTop = document.createElement("div");
    paddingTop.setAttribute("class", "float left");
    paddingTop.style.width = width + "px";
    paddingTop.style.height = polygon.minY + "px";
    element.appendChild(paddingTop);

    for (var y = polygon.minY; y < polygon.maxY; y += lineHeight) {
        var xIntercepts = polygonLineIntercepts(polygon, y, lineHeight);
        var left = xIntercepts[0];
        var right = xIntercepts[1];

        var paddingLeft = document.createElement("div");
        paddingLeft.setAttribute("class", "float left");
        paddingLeft.style.width = left + "px";
        element.appendChild(paddingLeft);

        var paddingRight = document.createElement("div");
        paddingRight.setAttribute("class", "float right");
        paddingRight.style.width = width - right + "px";
        element.appendChild(paddingRight);
    }

    var text = document.createTextNode(generatePolygonContentString(polygon, lineHeight));
    var p = document.createElement("p");
    p.appendChild(text);

    var element = document.getElementById(elementId);
    element.appendChild(p);
}

function positionInformativeText(elementId, stylesheet, polygon, lineHeight)
{
    stylesheet.insertRule("#" + elementId + " { position: absolute; top: " + (polygon.maxY + lineHeight) + "px;}");
}

function createPolygonShapeInsideTestCase() {
    var stylesheet = document.getElementById("stylesheet").sheet;
    var polygon = createPolygon(vertices);
    generatePolygonShapeInsideElement("polygon-shape-inside", stylesheet, polygon, lineHeight);
    generatePolygonSVGElements("polygon-svg-shape", stylesheet, polygon, lineHeight);
    positionInformativeText("informative-text", stylesheet, polygon, lineHeight)
}

function createPolygonShapeInsideTestCaseExpected() {
    var stylesheet = document.getElementById("stylesheet").sheet;
    var polygon = createPolygon(vertices);
    generateSimulatedPolygonShapeInsideElement("polygon-shape-inside", stylesheet, polygon, lineHeight);
    generatePolygonSVGElements("polygon-svg-shape", stylesheet, polygon, lineHeight);
    positionInformativeText("informative-text", stylesheet, polygon, lineHeight)
}
