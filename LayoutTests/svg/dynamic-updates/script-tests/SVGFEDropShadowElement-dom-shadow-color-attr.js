// [Name] SVGFEDropShadowElement-dom-shadow-color-attr.js
// [Expected rendering result] A green circle with a green shadow - and a series of PASS messages

description("Tests dynamic updates of the 'flood-color' attribute of the SVGFEDropShadowElement object")
createSVGTestCase();

var dropShadowElement = createSVGElement("feDropShadow");
dropShadowElement.setAttribute("dx", "10");
dropShadowElement.setAttribute("dy", "10");
dropShadowElement.setAttribute("stdDeviation", "0");
dropShadowElement.setAttribute("flood-color", "black");

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

shouldBeEqualToString("dropShadowElement.getAttribute('flood-color')", "black");

function repaintTest() {
    dropShadowElement.setAttribute("flood-color", "green");
    shouldBeEqualToString("dropShadowElement.getAttribute('flood-color')", "green");

    completeTest();
}

var successfullyParsed = true;
