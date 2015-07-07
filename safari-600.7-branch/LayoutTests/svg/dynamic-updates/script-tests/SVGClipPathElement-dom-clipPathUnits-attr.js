// [Name] SVGClipPathElement-dom-clipPathUnits-attr.js
// [Expected rendering result] green circle - and a series of PASS messages

description("Tests dynamic updates of the 'clipPathUnits' attribute of the SVGClipPathElement object")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var clipPathElement = createSVGElement("clipPath");
clipPathElement.setAttribute("id", "clipper");
clipPathElement.setAttribute("clipPathUnits", "userSpaceOnUse");

var rectElementA = createSVGElement("rect");
rectElementA.setAttribute("width", "10");
rectElementA.setAttribute("height", "10");
clipPathElement.appendChild(rectElementA);

defsElement.appendChild(clipPathElement);

var rectElementB = createSVGElement("rect");
rectElementB.setAttribute("width", "100");
rectElementB.setAttribute("height", "100");
rectElementB.setAttribute("fill", "green");
rectElementB.setAttribute("clip-path", "url(#clipper)");
rootSVGElement.appendChild(rectElementB);

shouldBeEqualToString("clipPathElement.getAttribute('clipPathUnits')", "userSpaceOnUse")

function repaintTest() {
    clipPathElement.setAttribute("clipPathUnits", "objectBoundingBox");
    shouldBeEqualToString("clipPathElement.getAttribute('clipPathUnits')", "objectBoundingBox");

    completeTest();
}

var successfullyParsed = true;
