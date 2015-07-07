// [Name] SVGFEComponentTransferElement-svgdom-amplitude-prop.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'amplitude' property of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("amplitude", "3");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("amplitude", "3");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("amplitude", "3");

var feAFunc = createSVGElement("feFuncA");
feAFunc.setAttribute("type", "gamma");
feAFunc.setAttribute("amplitude", "3");

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

shouldBe("feRFunc.amplitude.baseVal", "3");
shouldBe("feRFunc.amplitude.baseVal", "3");
shouldBe("feRFunc.amplitude.baseVal", "3");
shouldBe("feRFunc.amplitude.baseVal", "3");

function repaintTest() {
    feRFunc.amplitude.baseVal = 1;
    feGFunc.amplitude.baseVal = 1;
    feBFunc.amplitude.baseVal = 1;
    feAFunc.amplitude.baseVal = 1;

    shouldBe("feRFunc.amplitude.baseVal", "1");
    shouldBe("feGFunc.amplitude.baseVal", "1");
    shouldBe("feBFunc.amplitude.baseVal", "1");
    shouldBe("feAFunc.amplitude.baseVal", "1");

    completeTest();
}

var successfullyParsed = true;
