// [Name] SVGFEDropShadowElement-svgdom-shddow-opacity-prop.js
// [Expected rendering result] A green circle with a half transparent black shadow - and a series of PASS messages

description("Tests dynamic updates of the 'flood-opacity' property of the SVGFEDropShadowElement object")
createSVGTestCase();

var dropShadowElement = createSVGElement("feDropShadow");
dropShadowElement.setAttribute("dx", "10");
dropShadowElement.setAttribute("dy", "10");
dropShadowElement.setAttribute("stdDeviation", "0");
dropShadowElement.setAttribute("flood-opacity", "0");

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

shouldBeEqualToString("parseFloat(document.defaultView.getComputedStyle(dropShadowElement).getPropertyValue('flood-opacity')).toFixed(1)", "0.0");

function repaintTest() {
    dropShadowElement.style.setProperty("flood-opacity", ".5", "");
    shouldBeEqualToString("parseFloat(document.defaultView.getComputedStyle(dropShadowElement).getPropertyValue('flood-opacity')).toFixed(1)", "0.5");

    completeTest();
}

var successfullyParsed = true;
