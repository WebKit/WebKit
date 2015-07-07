// [Name] SVGFEConvolveMatrixElement-svgdom-order-prop.js
// [Expected rendering result] An image with feConvolveMatrix filter - and a series of PASS messages

description("Tests dynamic updates of the 'order' property of the SVGFEConvolveMatrixElement object")
createSVGTestCase();

var convolveMatrixElement = createSVGElement("feConvolveMatrix");
convolveMatrixElement.setAttribute("in", "SourceGraphic");
convolveMatrixElement.setAttribute("order", "2 2");
convolveMatrixElement.setAttribute("kernelMatrix", "3 0 3 0");

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
rootSVGElement.setAttribute("height", "200");

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/convolveImage.png");
imageElement.setAttribute("x", "0");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "200");
imageElement.setAttribute("height", "200");
imageElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(imageElement);

shouldBe("convolveMatrixElement.orderX.baseVal", "2");
shouldBe("convolveMatrixElement.orderY.baseVal", "2");

function repaintTest() {
    convolveMatrixElement.setAttribute("kernelMatrix", "3 0 3 0 0 0 3 0 3");
    convolveMatrixElement.orderX.baseVal = 3;
    convolveMatrixElement.orderY.baseVal = 3;
    shouldBe("convolveMatrixElement.orderX.baseVal", "3");
    shouldBe("convolveMatrixElement.orderY.baseVal", "3");

    completeTest();
}

var successfullyParsed = true;
