// [Name] SVGForeignObjectElement-dom-height-attr.js
// [Expected rendering result] unclipped 'Test passed' text - and a series of PASS messages

description("Tests dynamic updates of the 'height' attribute of the SVGForeignObjectElement object")
createSVGTestCase();

var foreignObjectElement = createSVGElement("foreignObject");
foreignObjectElement.setAttribute("x", "100");
foreignObjectElement.setAttribute("y", "80");
foreignObjectElement.setAttribute("width", "150");
foreignObjectElement.setAttribute("height", "2");

var htmlDivElement = document.createElementNS(xhtmlNS, "xhtml:div");
htmlDivElement.setAttribute("style", "background-color: green; color: white; text-align: center");
htmlDivElement.textContent = "Test passed";

foreignObjectElement.appendChild(htmlDivElement);
rootSVGElement.appendChild(foreignObjectElement);

shouldBeEqualToString("foreignObjectElement.getAttribute('height')", "2");

function executeTest() {
    foreignObjectElement.setAttribute("height", "150");
    shouldBeEqualToString("foreignObjectElement.getAttribute('height')", "150");

    completeTest();
}

startTest(foreignObjectElement, 150, 81);

var successfullyParsed = true;
