// [Name] SVGFEConvolveMatrixElement-dom-bias-attr.js
// [Expected rendering result] An image with feConvolveMatrix filter - and a series of PASS messages

description("Tests dynamic updates of the 'bias' attribute of the SVGFEConvolveMatrixElement object")
createSVGTestCase();

var convolveMatrixElement = createSVGElement("feConvolveMatrix");
convolveMatrixElement.setAttribute("in", "SourceGraphic");
convolveMatrixElement.setAttribute("order", "3");
convolveMatrixElement.setAttribute("kernelMatrix", "3 0 3 0 0 0 3 0 3");
convolveMatrixElement.setAttribute("bias", "0.5");

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

shouldBeEqualToString("convolveMatrixElement.getAttribute('bias')", "0.5");

function repaintTest() {
    convolveMatrixElement.setAttribute("bias", "0");
    shouldBeEqualToString("convolveMatrixElement.getAttribute('bias')", "0");

    completeTest();
}

var successfullyParsed = true;
