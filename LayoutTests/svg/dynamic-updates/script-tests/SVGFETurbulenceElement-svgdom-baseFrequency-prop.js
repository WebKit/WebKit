// [Name] SVGFESpotTurbulenceElement-svgdom-baseFrequency-prop.js
// [Expected rendering result] An image with turbulence filter - and a series of PASS messages

description("Tests dynamic updates of the 'baseFrequency' property of the SVGFETurbulenceElment object")
createSVGTestCase();

var turbulence = createSVGElement("feTurbulence");
turbulence.setAttribute("baseFrequency", "0.07");
turbulence.setAttribute("numOctaves", "3");
turbulence.setAttribute("seed", "5");
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

<!-- We should multiply and round the value of baseFrequency otherwise the expected value cannot be precisely represented as a floating point number and the comparison will fail.-->
shouldBe("Math.round(turbulence.baseFrequencyX.baseVal * 1000)", "70");
shouldBe("Math.round(turbulence.baseFrequencyY.baseVal * 1000)", "70");

function executeTest() {
    turbulence.baseFrequencyX.baseVal = 0.05;
    turbulence.baseFrequencyY.baseVal = 0.05;
    shouldBe("Math.round(turbulence.baseFrequencyX.baseVal * 1000)", "50");
    shouldBe("Math.round(turbulence.baseFrequencyY.baseVal * 1000)", "50");

    completeTest();
}

startTest(rectElement, 100, 100);

var successfullyParsed = true;
