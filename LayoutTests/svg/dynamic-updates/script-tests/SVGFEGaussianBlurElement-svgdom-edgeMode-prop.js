// [Name] SVGFEGaussianBlurElement-dom-edgeMode-prop.js
// [Expected rendering result] An image with feGaussianBlur filter - and a series of PASS messages

description("Tests dynamic updates of the 'edgeMode' property of the SVGFEGaussianBlurElement object")
createSVGTestCase();

var gaussianBlurElement = createSVGElement("feGaussianBlur");
gaussianBlurElement.setAttribute("in", "SourceGraphic");
gaussianBlurElement.setAttribute("stdDeviation", "10");
gaussianBlurElement.setAttribute("edgeMode", "wrap");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(gaussianBlurElement);

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

shouldBe("gaussianBlurElement.edgeMode.baseVal", "SVGFEGaussianBlurElement.SVG_EDGEMODE_WRAP");

function repaintTest() {
    gaussianBlurElement.edgeMode.baseVal = SVGFEGaussianBlurElement.SVG_EDGEMODE_DUPLICATE;
    shouldBe("gaussianBlurElement.edgeMode.baseVal", "SVGFEGaussianBlurElement.SVG_EDGEMODE_DUPLICATE");

    completeTest();
}

var successfullyParsed = true;
