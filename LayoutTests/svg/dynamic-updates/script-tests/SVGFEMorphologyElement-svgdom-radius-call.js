// [Name] SVGFEMorphologyElement-svgdom-radius-call.js
// [Expected rendering result] A text message with feMorphology filter - and a series of PASS messages

description("Tests dynamic updates of the 'radius' property of the SVGFEMorphologyElement object")
createSVGTestCase();

var morphologyElement = createSVGElement("feMorphology");
morphologyElement.setAttribute("in", "SourceGraphic");
morphologyElement.setAttribute("operator", "erode");
morphologyElement.setAttribute("radius", "6");

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

shouldBe("morphologyElement.radiusX.baseVal", "6");
shouldBe("morphologyElement.radiusY.baseVal", "6");

function executeTest() {
    morphologyElement.setRadius(4, 4);
    shouldBe("morphologyElement.radiusX.baseVal", "4");
    shouldBe("morphologyElement.radiusY.baseVal", "4");

    completeTest();
}

startTest(rootSVGElement, 100, 100);

var successfullyParsed = true;
