// [Name] SVGFEImageElement-svgdom-preserveAspectRatio-prop.js
// [Expected rendering result] A rectangle with feImage filter - and a series of PASS messages

description("Tests dynamic updates of the 'preserveAspectRatio' property of the SVGFEImageElement object")
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

shouldBe("image.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMINYMIN");
shouldBe("image.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");

function repaintTest() {
    image.preserveAspectRatio.baseVal.align = SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMINYMID;
    image.preserveAspectRatio.baseVal.meetOrSlice = SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE;
    shouldBe("image.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMINYMID");
    shouldBe("image.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");

    completeTest();
}

var successfullyParsed = true;
