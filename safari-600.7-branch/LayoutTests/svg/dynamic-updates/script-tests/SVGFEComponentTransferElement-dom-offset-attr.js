// [Name] SVGFEComponentTransferElement-dom-offset-attr.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'offset' attribute of the SVGFEComponentTransferElement object")
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

shouldBeEqualToString("feRFunc.getAttribute('offset')", "0.5");
shouldBeEqualToString("feGFunc.getAttribute('offset')", "0.5");
shouldBeEqualToString("feBFunc.getAttribute('offset')", "0.5");
shouldBeEqualToString("feAFunc.getAttribute('offset')", "0.5");

function repaintTest() {
    feRFunc.setAttribute("offset", "0.1");
	feGFunc.setAttribute("offset", "0.1");
	feBFunc.setAttribute("offset", "0.1");
	feAFunc.setAttribute("offset", "0.1");

    shouldBeEqualToString("feRFunc.getAttribute('offset')", "0.1");
	shouldBeEqualToString("feGFunc.getAttribute('offset')", "0.1");
	shouldBeEqualToString("feBFunc.getAttribute('offset')", "0.1");
	shouldBeEqualToString("feAFunc.getAttribute('offset')", "0.1");

    completeTest();
}

var successfullyParsed = true;
