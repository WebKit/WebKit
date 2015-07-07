// [Name] SVGClipPathElement-svgdom-clipPathUnits-prop.js
// [Expected rendering result] green circle - and a series of PASS messages

description("Tests dynamic updates of the 'clipPathUnits' property of the SVGClipPathElement object")
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

shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");

function repaintTest() {
    clipPathElement.clipPathUnits.baseVal = SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    shouldBe("clipPathElement.clipPathUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");

    completeTest();
}

var successfullyParsed = true;
