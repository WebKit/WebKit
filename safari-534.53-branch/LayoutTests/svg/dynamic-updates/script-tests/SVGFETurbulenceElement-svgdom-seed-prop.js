// [Name] SVGFESpotTurbulenceElement-svgdom-seed-prop.js
// [Expected rendering result] An image with turbulence filter - and a series of PASS messages

description("Tests dynamic updates of the 'seed' property of the SVGFETurbulenceElment object")
createSVGTestCase();

var turbulence = createSVGElement("feTurbulence");
turbulence.setAttribute("baseFrequency", "0.07");
turbulence.setAttribute("numOctaves", "3");
turbulence.setAttribute("seed", "10");
turbulence.setAttribute("stitchTiles", "noStitch");
turbulence.setAttribute("type", "turbulence");

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

shouldBe("turbulence.seed.baseVal", "10");

function executeTest() {
    turbulence.seed.baseVal = 5;
    shouldBe("turbulence.seed.baseVal", "5");

    completeTest();
}

startTest(rectElement, 100, 100);

var successfullyParsed = true;
