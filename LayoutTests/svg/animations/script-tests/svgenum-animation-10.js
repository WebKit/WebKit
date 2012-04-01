description("Test SVGMarkerUnitsType enumeration animations");
createSVGTestCase();

// Setup test document
var marker = createSVGElement("marker");
marker.setAttribute("id", "marker");
marker.setAttribute("viewBox", "0 0 10 10");
marker.setAttribute("markerWidth", "2");
marker.setAttribute("markerHeight", "2");
marker.setAttribute("refX", "5");
marker.setAttribute("refY", "5");
marker.setAttribute("markerUnits", "userSpaceOnUse");

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
animate1.setAttribute("attributeName", "markerUnits");
animate1.setAttribute("begin", "path.click");
animate1.setAttribute("dur", "4s");
animate1.setAttribute("from", "userSpaceOnUse");
animate1.setAttribute("to", "strokeWidth");
animate1.setAttribute("fill", "freeze");
marker.appendChild(animate1);

// Setup animation test
function sample1() {
    shouldBe("marker.markerUnits.animVal", "SVGMarkerElement.SVG_MARKERUNITS_USERSPACEONUSE");
    shouldBe("marker.markerUnits.baseVal", "SVGMarkerElement.SVG_MARKERUNITS_USERSPACEONUSE");
}

function sample2() {
    shouldBe("marker.markerUnits.animVal", "SVGMarkerElement.SVG_MARKERUNITS_STROKEWIDTH");
    shouldBe("marker.markerUnits.baseVal", "SVGMarkerElement.SVG_MARKERUNITS_USERSPACEONUSE");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.999, sample1],
        ["animation", 2.001, sample2],
        ["animation", 3.999, sample2],
        ["animation", 4.001, sample2]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
