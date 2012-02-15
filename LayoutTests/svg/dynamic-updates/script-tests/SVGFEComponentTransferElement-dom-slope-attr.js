// [Name] SVGFEComponentTransferElement-dom-slope-attr.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'slope' attribute of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("type", "linear");
feRFunc.setAttribute("slope", "1");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("type", "linear");
feGFunc.setAttribute("slope", "1");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("type", "linear");
feBFunc.setAttribute("slope", "1");

var feAFunc = createSVGElement("feFuncB");
feAFunc.setAttribute("type", "linear");
feAFunc.setAttribute("slope", "1");

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

shouldBeEqualToString("feRFunc.getAttribute('slope')", "1");
shouldBeEqualToString("feGFunc.getAttribute('slope')", "1");
shouldBeEqualToString("feBFunc.getAttribute('slope')", "1");
shouldBeEqualToString("feAFunc.getAttribute('slope')", "1");

function repaintTest() {
    feRFunc.setAttribute("slope", "2");
	feGFunc.setAttribute("slope", "2");
	feBFunc.setAttribute("slope", "2");
	feAFunc.setAttribute("slope", "2");

    shouldBeEqualToString("feRFunc.getAttribute('slope')", "2");
    shouldBeEqualToString("feGFunc.getAttribute('slope')", "2");
    shouldBeEqualToString("feBFunc.getAttribute('slope')", "2");
    shouldBeEqualToString("feAFunc.getAttribute('slope')", "2");

    completeTest();
}

var successfullyParsed = true;
