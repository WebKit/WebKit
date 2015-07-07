// [Name] SVGFEComponentTransferElement-dom-tableValues-attr.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'tableValues' attribute of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("type", "table");
feRFunc.setAttribute("tableValues", "0 1 1 0");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("type", "table");
feGFunc.setAttribute("tableValues", "0 1 1 0.5");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("type", "table");
feBFunc.setAttribute("tableValues", "0 0 1 0.1");

var feAFunc = createSVGElement("feFuncA");
feAFunc.setAttribute("type", "table");
feAFunc.setAttribute("tableValues", "0.5 10 1 0.5");

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

shouldBeEqualToString("feRFunc.getAttribute('tableValues')", "0 1 1 0");
shouldBeEqualToString("feGFunc.getAttribute('tableValues')", "0 1 1 0.5");
shouldBeEqualToString("feBFunc.getAttribute('tableValues')", "0 0 1 0.1");
shouldBeEqualToString("feAFunc.getAttribute('tableValues')", "0.5 10 1 0.5");

function repaintTest() {
    feRFunc.setAttribute("tableValues", "0 1 0.9 0");
	feGFunc.setAttribute("tableValues", "0 1 1 0.6");
	feBFunc.setAttribute("tableValues", "0 0 1 0.2");
	feAFunc.setAttribute("tableValues", "0.5 10 1 0.9");
    
	shouldBeEqualToString("feRFunc.getAttribute('tableValues')", "0 1 0.9 0");
	shouldBeEqualToString("feGFunc.getAttribute('tableValues')", "0 1 1 0.6");
	shouldBeEqualToString("feBFunc.getAttribute('tableValues')", "0 0 1 0.2");
	shouldBeEqualToString("feAFunc.getAttribute('tableValues')", "0.5 10 1 0.9");

    completeTest();
}

var successfullyParsed = true;
