description("Animate SVGMarkerElement orientAttr to an angle");
createSVGTestCase();

// Setup test document

var marker = createSVGElement("marker");
marker.setAttribute("id", "marker");
marker.setAttribute("viewBox", "0 0 10 10");
marker.setAttribute("markerWidth", "2");
marker.setAttribute("markerHeight", "2");
marker.setAttribute("refX", "5");
marker.setAttribute("refY", "5");
marker.setAttribute("markerUnits", "strokeWidth");

var markerPath = createSVGElement("path");
markerPath.setAttribute("fill", "blue");
markerPath.setAttribute("d", "M 5 0 L 10 10 L 0 10 Z");
marker.appendChild(markerPath);

var defsElement = createSVGElement("defs");
defsElement.appendChild(marker);
rootSVGElement.appendChild(defsElement);

var path = createSVGElement("path");
path.setAttribute("id", "path");
path.setAttribute("onclick", "executeTest()");
path.setAttribute("fill", "none");
path.setAttribute("stroke", "green");
path.setAttribute("stroke-width", "10");
path.setAttribute("marker-start", "url(#marker)");
path.setAttribute("marker-end", "url(#marker)");
path.setAttribute("d", "M 130 135 L 180 135 L 180 185");
path.setAttribute("transform", "translate(-130, -120)");
rootSVGElement.appendChild(path);

var animate1 = createSVGElement("animate");
animate1.setAttribute("id", "animation");
animate1.setAttribute("attributeName", "orient");
animate1.setAttribute("begin", "path.click");
animate1.setAttribute("dur", "4s");
animate1.setAttribute("to", "180deg");
animate1.setAttribute("fill", "freeze");
marker.appendChild(animate1);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("marker.orientAngle.animVal.value", "0");
    shouldBe("marker.orientAngle.baseVal.value", "0");

    shouldBe("marker.orientType.animVal", "SVGMarkerElement.SVG_MARKER_ORIENT_ANGLE");
    shouldBe("marker.orientType.baseVal", "SVGMarkerElement.SVG_MARKER_ORIENT_ANGLE");
}

function sample2() {
    shouldBeCloseEnough("marker.orientAngle.animVal.value", "90");
    shouldBe("marker.orientAngle.baseVal.value", "0");

    shouldBe("marker.orientType.animVal", "SVGMarkerElement.SVG_MARKER_ORIENT_ANGLE");
    shouldBe("marker.orientType.baseVal", "SVGMarkerElement.SVG_MARKER_ORIENT_ANGLE");
}

function sample3() {
    shouldBeCloseEnough("marker.orientAngle.animVal.value", "180");
    shouldBe("marker.orientAngle.baseVal.value", "0");

    shouldBe("marker.orientType.animVal", "SVGMarkerElement.SVG_MARKER_ORIENT_ANGLE");
    shouldBe("marker.orientType.baseVal", "SVGMarkerElement.SVG_MARKER_ORIENT_ANGLE");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
