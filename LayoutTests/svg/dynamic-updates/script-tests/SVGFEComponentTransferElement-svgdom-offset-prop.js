// [Name] SVGFEComponentTransferElement-svgdom-offset-prop.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'offset' property of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("offset", "0.5");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("type", "gamma");
feGFunc.setAttribute("offset", "0.5");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("type", "gamma");
feBFunc.setAttribute("offset", "0.5");

var feAFunc = createSVGElement("feFuncA");
feAFunc.setAttribute("type", "gamma");
feAFunc.setAttribute("offset", "0.5");

var feComponentTransferElement = createSVGElement("feComponentTransfer");
feComponentTransferElement.appendChild(feRFunc);
feComponentTransferElement.appendChild(feGFunc);
feComponentTransferElement.appendChild(feBFunc);
feComponentTransferElement.appendChild(feAFunc);

var compTranFilter = createSVGElement("filter");
compTranFilter.setAttribute("id", "compTranFilter");
compTranFilter.setAttribute("filterUnits", "objectBoundingBox");
compTranFilter.appendChild(feComponentTransferElement);

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

shouldBe("Math.round(feRFunc.offset.baseVal * 100)", "50");
shouldBe("Math.round(feGFunc.offset.baseVal * 100)", "50");
shouldBe("Math.round(feBFunc.offset.baseVal * 100)", "50");
shouldBe("Math.round(feAFunc.offset.baseVal * 100)", "50");

function repaintTest() {
    feRFunc.offset.baseVal = 0.1;
	feGFunc.offset.baseVal = 0.1;
	feBFunc.offset.baseVal = 0.1;
	feAFunc.offset.baseVal = 0.1;

    shouldBe("Math.round(feRFunc.offset.baseVal * 100)", "10");
	shouldBe("Math.round(feGFunc.offset.baseVal * 100)", "10");
	shouldBe("Math.round(feBFunc.offset.baseVal * 100)", "10");
	shouldBe("Math.round(feAFunc.offset.baseVal * 100)", "10");

    completeTest();
}

var successfullyParsed = true;
