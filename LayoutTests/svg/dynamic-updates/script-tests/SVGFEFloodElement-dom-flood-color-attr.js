// [Name] SVGFEFLoodElement-dom-flood-color-attr.js
// [Expected rendering result] A simple rect with feFlood filter - and a series of PASS messages

description("Tests dynamic updates of the 'flood-color' attribute of the SVGFEFloodElement object")
createSVGTestCase();

var floodElement = createSVGElement("feFlood");
floodElement.setAttribute("flood-color", "rgb(145, 32, 103)");
floodElement.setAttribute("flood-opacity", ".8");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(floodElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("width", "700");
rootSVGElement.setAttribute("height", "200");

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", 60);
rectElement.setAttribute("y", 100);
rectElement.setAttribute("width", 100);
rectElement.setAttribute("height", 100);
rectElement.setAttribute("stroke", "#AF1E9D");
rectElement.setAttribute("stroke-width", "8");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("floodElement.getAttribute('flood-color')", "rgb(145, 32, 103)");

function executeTest() {
    floodElement.setAttribute("flood-color", "rgb(64, 128, 103)");
    shouldBeEqualToString("floodElement.getAttribute('flood-color')", "rgb(64, 128, 103)");

    completeTest();
}

startTest(rootSVGElement, 100, 100);

var successfullyParsed = true;
