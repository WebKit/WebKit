// [Name] SVGFEColorMatrixElement-dom-type-attr.js
// [Expected rendering result] Five lines with feColorMatrix filter - and a series of PASS messages

description("Tests dynamic updates of the 'type' attribute of the SVGFEColorMatrixElement object")
createSVGTestCase();

var matrixElement = createSVGElement("feColorMatrix");
matrixElement.setAttribute("in", "SourceGraphic");
matrixElement.setAttribute("type", "saturate");
matrixElement.setAttribute("values", "0.25");

var saturateElement = createSVGElement("feColorMatrix");
saturateElement.setAttribute("in", "SourceGraphic");
saturateElement.setAttribute("type", "hueRotate");
saturateElement.setAttribute("values", "90");

var hueRotateElement = createSVGElement("feColorMatrix");
hueRotateElement.setAttribute("in", "SourceGraphic");
hueRotateElement.setAttribute("type", "luminanceToAlpha");
hueRotateElement.setAttribute("result", "a");

var luminanceToAlphaElement = createSVGElement("feColorMatrix");
luminanceToAlphaElement.setAttribute("in", "SourceGraphic");
luminanceToAlphaElement.setAttribute("type", "matrix");
luminanceToAlphaElement.setAttribute("values", ".33 .33 .33 0 0 .33 .33 .33 0 0 .33 .33 .33 0 0 .33 .33 .33 0 0");

var compositeElement = createSVGElement("feComposite");
compositeElement.setAttribute("in", "SourceGraphic");
compositeElement.setAttribute("in2", "a");
compositeElement.setAttribute("operator", "in");

var matrixFilter = createSVGElement("filter");
matrixFilter.setAttribute("id", "MatrixFilter");
matrixFilter.setAttribute("filterUnits", "objectBoundingBox");
matrixFilter.setAttribute("x", "-5%");
matrixFilter.setAttribute("y", "-5%");
matrixFilter.setAttribute("width", "110%");
matrixFilter.setAttribute("height", "110%");
matrixFilter.appendChild(matrixElement);

var saturatedFilter = createSVGElement("filter");
saturatedFilter.setAttribute("id", "SaturateFilter");
saturatedFilter.setAttribute("filterUnits", "objectBoundingBox");
saturatedFilter.setAttribute("x", "-5%");
saturatedFilter.setAttribute("y", "-5%");
saturatedFilter.setAttribute("width", "110%");
saturatedFilter.setAttribute("height", "110%");
saturatedFilter.appendChild(saturateElement);

var hueRotateFilter = createSVGElement("filter");
hueRotateFilter.setAttribute("id", "HueRotateFilter");
hueRotateFilter.setAttribute("filterUnits", "objectBoundingBox");
hueRotateFilter.setAttribute("x", "-5%");
hueRotateFilter.setAttribute("y", "-5%");
hueRotateFilter.setAttribute("width", "110%");
hueRotateFilter.setAttribute("height", "110%");
hueRotateFilter.appendChild(hueRotateElement);

var luminanceToAlphaFilter = createSVGElement("filter");
luminanceToAlphaFilter.setAttribute("id", "LuminanceToAlphaFilter");
luminanceToAlphaFilter.setAttribute("filterUnits", "objectBoundingBox");
luminanceToAlphaFilter.setAttribute("x", "-5%");
luminanceToAlphaFilter.setAttribute("y", "-5%");
luminanceToAlphaFilter.setAttribute("width", "110%");
luminanceToAlphaFilter.setAttribute("height", "110%");
luminanceToAlphaFilter.appendChild(luminanceToAlphaElement);
luminanceToAlphaFilter.appendChild(compositeElement);

var gradientElement = createSVGElement("linearGradient");
gradientElement.setAttribute("id", "MyGradient");
gradientElement.setAttribute("gradientUnits", "userSpaceOnUse");
gradientElement.setAttribute("x1", "20");
gradientElement.setAttribute("y1", "0");
gradientElement.setAttribute("x2", "760");
gradientElement.setAttribute("y2", "0");

var stopElement1 = createSVGElement("stop");
stopElement1.setAttribute("offset", "0");
stopElement1.setAttribute("stop-color", "#dd00dd");

var stopElement2 = createSVGElement("stop");
stopElement2.setAttribute("offset", ".33");
stopElement2.setAttribute("stop-color", "#22cc22");

var stopElement3 = createSVGElement("stop");
stopElement3.setAttribute("offset", ".67");
stopElement3.setAttribute("stop-color", "#400000");

var stopElement4 = createSVGElement("stop");
stopElement4.setAttribute("offset", "1");
stopElement4.setAttribute("stop-color", "#a0a0ff");

gradientElement.appendChild(stopElement1);
gradientElement.appendChild(stopElement2);
gradientElement.appendChild(stopElement3);
gradientElement.appendChild(stopElement4);

var defsElement = createSVGElement("defs");
defsElement.appendChild(saturatedFilter);
defsElement.appendChild(matrixFilter);
defsElement.appendChild(hueRotateFilter);
defsElement.appendChild(luminanceToAlphaFilter);
defsElement.appendChild(gradientElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("width", "800");
rootSVGElement.setAttribute("height", "350");

var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", 798);
rectElement.setAttribute("height", 320);
rectElement.setAttribute("x", "1");
rectElement.setAttribute("y", "1");
rectElement.setAttribute("fill", "none");
rectElement.setAttribute("stroke", "blue");
rectElement.setAttribute("filter", "url(#SaturateFilter)");

var unfilteredRect = createSVGElement("rect");
unfilteredRect.setAttribute("x", "20");
unfilteredRect.setAttribute("y", "20");
unfilteredRect.setAttribute("width", "760");
unfilteredRect.setAttribute("height", "30");
unfilteredRect.setAttribute("fill", "url(#MyGradient)");
rootSVGElement.appendChild(unfilteredRect);

var matrixRect = createSVGElement("rect");
matrixRect.setAttribute("x", "20");
matrixRect.setAttribute("y", "80");
matrixRect.setAttribute("width", "760");
matrixRect.setAttribute("height", "30");
matrixRect.setAttribute("fill", "url(#MyGradient)");
matrixRect.setAttribute("filter", "url(#MatrixFilter)");
rootSVGElement.appendChild(matrixRect);

var saturatedRect = createSVGElement("rect");
saturatedRect.setAttribute("x", "20");
saturatedRect.setAttribute("y", "140");
saturatedRect.setAttribute("width", "760");
saturatedRect.setAttribute("height", "30");
saturatedRect.setAttribute("fill", "url(#MyGradient)");
saturatedRect.setAttribute("filter", "url(#SaturateFilter)");
rootSVGElement.appendChild(saturatedRect);

var hueRotateRect = createSVGElement("rect");
hueRotateRect.setAttribute("x", "20");
hueRotateRect.setAttribute("y", "200");
hueRotateRect.setAttribute("width", "760");
hueRotateRect.setAttribute("height", "30");
hueRotateRect.setAttribute("fill", "url(#MyGradient)");
hueRotateRect.setAttribute("filter", "url(#HueRotateFilter)");
rootSVGElement.appendChild(hueRotateRect);

var luminanceToAlphaRect = createSVGElement("rect");
luminanceToAlphaRect.setAttribute("x", "20");
luminanceToAlphaRect.setAttribute("y", "260");
luminanceToAlphaRect.setAttribute("width", "760");
luminanceToAlphaRect.setAttribute("height", "30");
luminanceToAlphaRect.setAttribute("fill", "url(#MyGradient)");
luminanceToAlphaRect.setAttribute("filter", "url(#LuminanceToAlphaFilter)");
rootSVGElement.appendChild(luminanceToAlphaRect);

rootSVGElement.appendChild(rectElement);
rootSVGElement.setAttribute("font-size", "20");
rootSVGElement.setAttribute("font-family", "Verdana");

shouldBeEqualToString("matrixElement.getAttribute('type')", "saturate");
shouldBeEqualToString("saturateElement.getAttribute('type')", "hueRotate");
shouldBeEqualToString("hueRotateElement.getAttribute('type')", "luminanceToAlpha");
shouldBeEqualToString("luminanceToAlphaElement.getAttribute('type')", "matrix");

function repaintTest() {
    matrixElement.setAttribute("type", "matrix");
    matrixElement.setAttribute("values", ".33 .33 .33 0 0 .33 .33 .33 0 0 .33 .33 .33 0 0 .33 .33 .33 0 0");

    saturateElement.setAttribute("type", "saturate");
    saturateElement.setAttribute("values", "0.25");

    hueRotateElement.setAttribute("type", "hueRotate");
    hueRotateElement.setAttribute("values", "90");

    luminanceToAlphaElement.setAttribute("type", "luminanceToAlpha");
    luminanceToAlphaElement.setAttribute("result", "a");

    shouldBeEqualToString("matrixElement.getAttribute('type')", "matrix");
    shouldBeEqualToString("saturateElement.getAttribute('type')", "saturate");
    shouldBeEqualToString("hueRotateElement.getAttribute('type')", "hueRotate");
    shouldBeEqualToString("luminanceToAlphaElement.getAttribute('type')", "luminanceToAlpha");

    completeTest();
}

var successfullyParsed = true;
