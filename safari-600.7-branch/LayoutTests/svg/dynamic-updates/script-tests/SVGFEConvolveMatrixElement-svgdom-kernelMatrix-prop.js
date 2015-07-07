// [Name] SVGFEConvolveMatrixElement-svgdom-kernelMatrix-prop.js
// [Expected rendering result] An image with feConvolveMatrix filter - and a series of PASS messages

// SVGNumberListToString converts a list to a string.
function SVGNumberListToString(list) {
    var result = "";
    for (var i = 0; i < list.numberOfItems; ++i) {
        // We should multiply and round the value of listItem otherwise the expected value cannot be precisely represented as a floating point
        // number and later the comparison will fail.
        var item = Math.round(list.getItem(i).value * 1000) / 1000;
        result += item;
        result += " ";
    }
    return result;
}

description("Tests dynamic updates of the 'kernelMatrix' property of the SVGFEConvolveMatrixElement object")
createSVGTestCase();

var convolveMatrixElement = createSVGElement("feConvolveMatrix");
convolveMatrixElement.setAttribute("in", "SourceGraphic");
convolveMatrixElement.setAttribute("order", "3");
convolveMatrixElement.setAttribute("kernelMatrix", "-2 0 0 0 1 0 0 0 2");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(convolveMatrixElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("width", "700");
rootSVGElement.setAttribute("height", "200");

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/convolveImage.png");
imageElement.setAttribute("x", "0");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "200");
imageElement.setAttribute("height", "200");
imageElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(imageElement);

shouldBeEqualToString("SVGNumberListToString(convolveMatrixElement.kernelMatrix.baseVal)", "-2 0 0 0 1 0 0 0 2 ");

function repaintTest() {
    var number = rootSVGElement.createSVGNumber();
    number.value = 3;
    convolveMatrixElement.kernelMatrix.baseVal.replaceItem(number, 0);

    shouldBeEqualToString("SVGNumberListToString(convolveMatrixElement.kernelMatrix.baseVal)", "3 0 0 0 1 0 0 0 2 ");

    completeTest();
}

var successfullyParsed = true;
