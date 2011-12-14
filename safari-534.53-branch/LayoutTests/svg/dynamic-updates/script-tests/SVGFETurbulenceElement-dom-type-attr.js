// [Name] SVGFETurbulenceElement-dom-type-attr.js
// [Expected rendering result] An image with turbulence filter - and a series of PASS messages

description("Tests dynamic updates of the 'dom' attribute of the SVGFETurbulenceElement object")
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
rectElement.setAttribute("width", 200);
rectElement.setAttribute("height", 200);
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("turbulence.getAttribute('type')", "fractalNoise");

function executeTest() {
    turbulence.setAttribute("type", "turbulence");
    shouldBeEqualToString("turbulence.getAttribute('type')", "turbulence");

    completeTest();
}

startTest(rectElement, 100, 100);

var successfullyParsed = true;
