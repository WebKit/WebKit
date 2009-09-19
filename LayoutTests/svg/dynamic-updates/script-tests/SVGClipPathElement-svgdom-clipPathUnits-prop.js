// [Name] SVGClipPathElement-svgdom-clipPathUnits-prop.js
// [Expected rendering result] green circle - and a series of PASS messages

description("Tests dynamic updates of the 'clipPathUnits' property of the SVGClipPathElement object")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var clipPathElement = createSVGElement("clipPath");
clipPathElement.setAttribute("id", "clipper");
clipPathElement.setAttribute("clipPathUnits", "objectBoundingBox");

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "150");
circleElement.setAttribute("r", "150");
clipPathElement.appendChild(circleElement);

defsElement.appendChild(clipPathElement);;

var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "300");
rectElement.setAttribute("height", "300");
rectElement.setAttribute("fill", "green");
rectElement.setAttribute("clip-path", "url(#clipper)");
rootSVGElement.appendChild(rectElement);

shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");

function executeTest() {
    clipPathElement.clipPathUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE;
    shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

    completeTest();
}

startTest(rectElement, 150, 150);

var successfullyParsed = true;
