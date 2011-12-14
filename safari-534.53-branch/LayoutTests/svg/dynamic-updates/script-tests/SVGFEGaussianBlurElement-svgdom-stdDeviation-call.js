// [Name] SVGFEGaussianBlurElement-dom-stdDeviation-prop.js
// [Expected rendering result] A simple rectangle with feGaussianBlur filter - and a series of PASS messages

description("Tests dynamic updates of the 'stdDeviation' property of the SVGFEGaussianBlurElement object")
createSVGTestCase();

var blurElement = createSVGElement("feGaussianBlur");
blurElement.setAttribute("stdDeviation", "5");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(blurElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("width", "700");
rootSVGElement.setAttribute("height", "200");

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", 60);
rectElement.setAttribute("y", 20);
rectElement.setAttribute("width", 100);
rectElement.setAttribute("height", 100);
rectElement.setAttribute("stroke", "#AF1E9D");
rectElement.setAttribute("stroke-width", "8");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBe("blurElement.stdDeviationX.baseVal", "5");
shouldBe("blurElement.stdDeviationY.baseVal", "5");

function executeTest() {
    blurElement.setStdDeviation(10, 10);
    shouldBe("blurElement.stdDeviationX.baseVal", "10");
    shouldBe("blurElement.stdDeviationY.baseVal", "10");

    completeTest();
}

startTest(rootSVGElement, 100, 100);

var successfullyParsed = true;
