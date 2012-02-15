// [Name] SVGFEConvolveMatrixElement-svgdom-kernelUnitLength-prop.js
// [Expected rendering result] An image with feConvolveMatrix filter - and a series of PASS messages

description("Tests dynamic updates of the 'kernelUnitLength' property of the SVGFEConvolveMatrixElement object")
createSVGTestCase();

var convolveMatrixElement = createSVGElement("feConvolveMatrix");
convolveMatrixElement.setAttribute("in", "SourceGraphic");
convolveMatrixElement.setAttribute("order", "3");
convolveMatrixElement.setAttribute("kernelMatrix", "3 0 5 0 0 0 3 0 3");
convolveMatrixElement.setAttribute("kernelUnitLength", "1");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(convolveMatrixElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("width", "700");
rootSVGElement.setAttribute("height", "250");

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/convolveImage.png");
imageElement.setAttribute("x", "0");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "200");
imageElement.setAttribute("height", "200");
imageElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(imageElement);

shouldBe("convolveMatrixElement.kernelUnitLengthX.baseVal", "1");
shouldBe("convolveMatrixElement.kernelUnitLengthY.baseVal", "1");

function repaintTest() {
    convolveMatrixElement.kernelUnitLengthX.baseVal = 0.05;
    convolveMatrixElement.kernelUnitLengthY.baseVal = 0.05;
    shouldBe("Math.round(convolveMatrixElement.kernelUnitLengthX.baseVal * 1000)", "50");
    shouldBe("Math.round(convolveMatrixElement.kernelUnitLengthY.baseVal * 1000)", "50");

    completeTest();
}

var successfullyParsed = true;
