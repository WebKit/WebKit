// [Name] SVGFEDropShadowElement-dom-stdDeviation-attr.js
// [Expected rendering result] A simple circle with a black shadow - and a series of PASS messages

description("Tests dynamic updates of the 'stdDeviation' attribute of the SVGFEDropShadowElement object")
createSVGTestCase();

var dropShadowElement = createSVGElement("feDropShadow");
dropShadowElement.setAttribute("stdDeviation", "0");
dropShadowElement.setAttribute("dx", "0");
dropShadowElement.setAttribute("dy", "0");

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

shouldBeEqualToString("dropShadowElement.getAttribute('stdDeviation')", "0");

function repaintTest() {
    dropShadowElement.setAttribute("stdDeviation", "10");
    shouldBeEqualToString("dropShadowElement.getAttribute('stdDeviation')", "10");

    completeTest();
}

var successfullyParsed = true;
