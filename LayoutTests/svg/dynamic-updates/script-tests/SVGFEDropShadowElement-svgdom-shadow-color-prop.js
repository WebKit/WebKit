// [Name] SVGFEDropShadowElement-svgdom-shadow-color-prop.js
// [Expected rendering result] A green circle with a green shadow - and a series of PASS messages

description("Tests dynamic updates of the 'flood-color' property of the SVGFEFloodElement object")
createSVGTestCase();

var dropShadowElement = createSVGElement("feDropShadow");
dropShadowElement.setAttribute("dx", "10");
dropShadowElement.setAttribute("dy", "10");
dropShadowElement.setAttribute("stdDeviation", "0");
dropShadowElement.setAttribute("flood-color", "rgb(0, 0, 0)");

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

shouldBeEqualToString("document.defaultView.getComputedStyle(dropShadowElement).getPropertyValue('flood-color')", "rgb(0, 0, 0)");

function repaintTest() {
    dropShadowElement.style.setProperty("flood-color", "rgb(0, 128, 0)", "");
    shouldBeEqualToString("document.defaultView.getComputedStyle(dropShadowElement).getPropertyValue('flood-color')", "rgb(0, 128, 0)");

    completeTest();
}

var successfullyParsed = true;
