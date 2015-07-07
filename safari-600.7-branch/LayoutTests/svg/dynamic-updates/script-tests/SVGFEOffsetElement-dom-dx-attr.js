// [Name] SVGFEOffsetElement-dom-dx-attr.js
// [Expected rendering result] A green circle in the center of the image - and a series of PASS messages

description("Tests dynamic updates of the 'dx' attribute of the SVGFEOffsetElement object")
createSVGTestCase();

var offsetElement = createSVGElement("feOffset");
offsetElement.setAttribute("dx", "50");
offsetElement.setAttribute("dy", "0");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
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

shouldBeEqualToString("offsetElement.getAttribute('dx')", "50");

function repaintTest() {
    offsetElement.setAttribute("dx", "0");
    shouldBeEqualToString("offsetElement.getAttribute('dx')", "0");

    completeTest();
}

var successfullyParsed = true;
