// [Name] SVGFEOffsetElement-svgdom-in-prop.js
// [Expected rendering result] A green circle in the center of the image - and a series of PASS messages

description("Tests dynamic updates of the 'in' property of the SVGFEOffsetElement object")
createSVGTestCase();

var dropShadowElement = createSVGElement("feDropShadow");
dropShadowElement.setAttribute("dx", "10");
dropShadowElement.setAttribute("dy", "10");
dropShadowElement.setAttribute("in", "SourceAlpha");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(dropShadowElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "100");
circleElement.setAttribute("cy", "100");
circleElement.setAttribute("r", "70");
circleElement.setAttribute("fill", "green");
circleElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(circleElement);

shouldBeEqualToString("dropShadowElement.in1.baseVal", "SourceAlpha");

function repaintTest() {
    dropShadowElement.in1.baseVal = "SourceGraphic";
    shouldBeEqualToString("dropShadowElement.in1.baseVal", "SourceGraphic");

    completeTest();
}

var successfullyParsed = true;
