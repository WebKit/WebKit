// [Name] SVGFEOffsetElement-svgdom-in-prop.js
// [Expected rendering result] A green circle in the center of the image - and a series of PASS messages

description("Tests dynamic updates of the 'in' property of the SVGFEOffsetElement object")
createSVGTestCase();

var blurElement = createSVGElement("feGaussianBlur");
blurElement.setAttribute("in", "SourceGraphic");
blurElement.setAttribute("stdDeviation", "2");
blurElement.setAttribute("result", "blur");

var offsetElement = createSVGElement("feOffset");
offsetElement.setAttribute("in", "blur");
offsetElement.setAttribute("dx", "0");
offsetElement.setAttribute("dy", "0");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(blurElement);
filterElement.appendChild(offsetElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);

var rectElement = createSVGElement("circle");
rectElement.setAttribute("width", 200);
rectElement.setAttribute("height", 200);
rectElement.setAttribute("cx", "100");
rectElement.setAttribute("cy", "100");
rectElement.setAttribute("r", "70");
rectElement.setAttribute("fill", "green");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("offsetElement.in1.baseVal", "blur");

function repaintTest() {
    offsetElement.in1.baseVal = "SourceGraphic";
    shouldBeEqualToString("offsetElement.in1.baseVal", "SourceGraphic");

    completeTest();
}

var successfullyParsed = true;
