// [Name] SVGFEComponentTransferElement-svgdom-exponent-prop.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'exponent' property of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("exponent", "2");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("type", "gamma");
feGFunc.setAttribute("exponent", "2");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("type", "gamma");
feBFunc.setAttribute("exponent", "2");

var feAFunc = createSVGElement("feFuncA");
feAFunc.setAttribute("type", "gamma");
feAFunc.setAttribute("exponent", "2");

var feCompnentTransferElement = createSVGElement("feComponentTransfer");
feCompnentTransferElement.appendChild(feRFunc);
feCompnentTransferElement.appendChild(feGFunc);
feCompnentTransferElement.appendChild(feBFunc);
feCompnentTransferElement.appendChild(feAFunc);

var compTranFilter = createSVGElement("filter");
compTranFilter.setAttribute("id", "compTranFilter");
compTranFilter.setAttribute("filterUnits", "objectBoundingBox");
compTranFilter.setAttribute("x", "0%");
compTranFilter.setAttribute("y", "0%");
compTranFilter.setAttribute("width", "100%");
compTranFilter.setAttribute("height", "100%");
compTranFilter.appendChild(feCompnentTransferElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(compTranFilter);
rootSVGElement.appendChild(defsElement);

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/struct-image-01.png");
imageElement.setAttribute("width", "400");
imageElement.setAttribute("height", "200");
imageElement.setAttribute("preserveAspectRatio", "none");
imageElement.setAttribute("filter", "url(#compTranFilter)");
rootSVGElement.appendChild(imageElement);

rootSVGElement.setAttribute("width", "400");
rootSVGElement.setAttribute("height", "200");

shouldBe("feRFunc.exponent.baseVal", "2");
shouldBe("feGFunc.exponent.baseVal", "2");
shouldBe("feBFunc.exponent.baseVal", "2");
shouldBe("feAFunc.exponent.baseVal", "2");

function repaintTest() {
    feRFunc.exponent.baseVal = 1;
    feGFunc.exponent.baseVal = 1
    feBFunc.exponent.baseVal = 1
    feAFunc.exponent.baseVal = 1

    shouldBe("feRFunc.exponent.baseVal", "1");
    shouldBe("feGFunc.exponent.baseVal", "1");
    shouldBe("feBFunc.exponent.baseVal", "1");
    shouldBe("feAFunc.exponent.baseVal", "1");

    completeTest();
}

var successfullyParsed = true;
