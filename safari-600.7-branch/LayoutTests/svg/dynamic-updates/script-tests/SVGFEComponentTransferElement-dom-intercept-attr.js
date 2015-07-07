// [Name] SVGFEComponentTransferElement-dom-intercept-attr.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'intercept' attribute of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("type", "linear");
feRFunc.setAttribute("intercept", "0.2");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("type", "linear");
feGFunc.setAttribute("intercept", "0.2");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("type", "linear");
feBFunc.setAttribute("intercept", "0.2");

var feAFunc = createSVGElement("feFuncA");
feAFunc.setAttribute("type", "linear");
feAFunc.setAttribute("intercept", "0.2");

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

shouldBeEqualToString("feRFunc.getAttribute('intercept')", "0.2");
shouldBeEqualToString("feGFunc.getAttribute('intercept')", "0.2");
shouldBeEqualToString("feBFunc.getAttribute('intercept')", "0.2");
shouldBeEqualToString("feAFunc.getAttribute('intercept')", "0.2");

function repaintTest() {
    feRFunc.setAttribute("intercept", "0.1");
	feGFunc.setAttribute("intercept", "0.1");
	feBFunc.setAttribute("intercept", "0.1");
	feAFunc.setAttribute("intercept", "0.1");
    
	shouldBeEqualToString("feRFunc.getAttribute('intercept')", "0.1");
	shouldBeEqualToString("feGFunc.getAttribute('intercept')", "0.1");
	shouldBeEqualToString("feBFunc.getAttribute('intercept')", "0.1");
	shouldBeEqualToString("feAFunc.getAttribute('intercept')", "0.1");

    completeTest();
}

var successfullyParsed = true;
