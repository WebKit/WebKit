// [Name] SVGForeignObjectElement-dom-x-attr.js
// [Expected rendering result] unclipped 'Test passed' text - and a series of PASS messages

description("Tests dynamic updates of the 'x' attribute of the SVGForeignObjectElement object")
createSVGTestCase();

var foreignObjectElement = createSVGElement("foreignObject");
foreignObjectElement.setAttribute("x", "-100");
foreignObjectElement.setAttribute("y", "80");
foreignObjectElement.setAttribute("width", "150");
foreignObjectElement.setAttribute("height", "150");

var htmlDivElement = document.createElementNS(xhtmlNS, "xhtml:div");
htmlDivElement.setAttribute("style", "background-color: green; color: white; text-align: center");
htmlDivElement.textContent = "Test passed";

foreignObjectElement.appendChild(htmlDivElement);
rootSVGElement.appendChild(foreignObjectElement);

shouldBeEqualToString("foreignObjectElement.getAttribute('x')", "-100");

function executeTest() {
    foreignObjectElement.setAttribute("x", "100");
    shouldBeEqualToString("foreignObjectElement.getAttribute('x')", "100");

    completeTest();
}

startTest(foreignObjectElement, 10, 100);

var successfullyParsed = true;
