// [Name] SVGFEComponentTransferElement-dom-amplitude-attr.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'amplitude' attribute of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("id", "fr");
feRFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("amplitude", "3");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("id", "fg");
feGFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("amplitude", "3");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("id", "fb");
feBFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("amplitude", "3");

var feAFunc = createSVGElement("feFuncA");
feAFunc.setAttribute("id", "fb");
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

shouldBeEqualToString("feRFunc.getAttribute('amplitude')", "3");
shouldBeEqualToString("feRFunc.getAttribute('amplitude')", "3");
shouldBeEqualToString("feRFunc.getAttribute('amplitude')", "3");
shouldBeEqualToString("feRFunc.getAttribute('amplitude')", "3");

function repaintTest() {
    feRFunc.setAttribute("amplitude", "1");
	feGFunc.setAttribute("amplitude", "1");
	feBFunc.setAttribute("amplitude", "1");
	feAFunc.setAttribute("amplitude", "1");

    shouldBeEqualToString("feRFunc.getAttribute('amplitude')", "1");
	shouldBeEqualToString("feGFunc.getAttribute('amplitude')", "1");
	shouldBeEqualToString("feBFunc.getAttribute('amplitude')", "1");
	shouldBeEqualToString("feAFunc.getAttribute('amplitude')", "1");

    completeTest();
}

var successfullyParsed = true;
