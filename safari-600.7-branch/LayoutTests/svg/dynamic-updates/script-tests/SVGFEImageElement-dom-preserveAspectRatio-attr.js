// [Name] SVGFEImageElement-dom-preserveAspectRatio-attr.js
// [Expected rendering result] A rectangle with feImage filter - and a series of PASS messages

description("Tests dynamic updates of the 'preserveAspectRatio' attribute of the SVGFEImageElement object")
createSVGTestCase();

var image = createSVGElement("feImage");
image.setAttribute("width", "300");
image.setAttribute("height", "200");
image.setAttribute("preserveAspectRatio", "xMinYMin slice");
image.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/image1.jpg");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.appendChild(image);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("height", "200");

var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "190");
rectElement.setAttribute("height", "190");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

rootSVGElement.setAttribute("viewBox", "0 0 400 150");

shouldBeEqualToString("image.getAttribute('preserveAspectRatio')", "xMinYMin slice");

function repaintTest() {
    image.setAttribute("preserveAspectRatio", "xMinYMid slice");
    shouldBeEqualToString("image.getAttribute('preserveAspectRatio')", "xMinYMid slice");

    completeTest();
}

var successfullyParsed = true;
