// [Name] SVGFEMorphologyElement-svgdom-operator-prop.js
// [Expected rendering result] A text message with feMorphology filter - and a series of PASS messages

description("Tests dynamic updates of the 'operator' property of the SVGFEMorphologyElement object")
createSVGTestCase();

var morphologyElement = createSVGElement("feMorphology");
morphologyElement.setAttribute("in", "SourceGraphic");
morphologyElement.setAttribute("operator", "dilate");
morphologyElement.setAttribute("radius", "4");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "700");
filterElement.setAttribute("height", "200");
filterElement.appendChild(morphologyElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("width", "700");
rootSVGElement.setAttribute("height", "200");
rootSVGElement.setAttribute("font-family", "Verdana");
rootSVGElement.setAttribute("font-size", "45");
rootSVGElement.setAttribute("fill", "none");
rootSVGElement.setAttribute("stroke", "black");
rootSVGElement.setAttribute("stroke-width", "8");

var textElement = createSVGElement("text");
textElement.setAttribute("x", 20);
textElement.setAttribute("y", 90);
textElement.textContent = "Erode radius = 4";
textElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(textElement);

shouldBe("morphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_DILATE");

function executeTest() {
    morphologyElement.operator.baseVal = SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE;
    shouldBe("morphologyElement.operator.baseVal", "SVGFEMorphologyElement.SVG_MORPHOLOGY_OPERATOR_ERODE");

    completeTest();
}

startTest(rootSVGElement, 100, 100);

var successfullyParsed = true;
