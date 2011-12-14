// [Name] SVGFESpotTurbulenceElement-svgdom-numOctaves-prop.js
// [Expected rendering result] An image with turbulence filter - and a series of PASS messages

description("Tests dynamic updates of the 'type' property of the SVGFETurbulenceElment object")
createSVGTestCase();

var turbulence = createSVGElement("feTurbulence");
turbulence.setAttribute("baseFrequency", "0.05");
turbulence.setAttribute("numOctaves", "3");
turbulence.setAttribute("seed", "5");
turbulence.setAttribute("stitchTiles", "noStitch");
turbulence.setAttribute("type", "fractalNoise");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(turbulence);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);

var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBe("turbulence.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE");

function executeTest() {
    turbulence.type.baseVal = SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE;
    shouldBe("turbulence.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE");

    completeTest();
}

startTest(rectElement, 100, 100);

var successfullyParsed = true;
