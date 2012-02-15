// [Name] SVGFEDropShadowElement-dom-dy-attr.js
// [Expected rendering result] A green circle in the center of the image and a black shadow with offset - and a series of PASS messages

description("Tests dynamic updates of the 'dy' attribute of the SVGFEDropShadowElement object")
createSVGTestCase();

var dropShadowElement = createSVGElement("feDropShadow");
dropShadowElement.setAttribute("dx", "10");
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

shouldBeEqualToString("dropShadowElement.getAttribute('dy')", "0");

function repaintTest() {
    dropShadowElement.setAttribute("dy", "10");
    shouldBeEqualToString("dropShadowElement.getAttribute('dy')", "10");

    completeTest();
}

var successfullyParsed = true;
