// [Name] SVGForeignObjectElement-dom-y-attr.js
// [Expected rendering result] 'Test passed' text - and a series of PASS mesages

description("Tests dynamic updates of the 'y' attribute of the SVGForeignObjectElement object")
createSVGTestCase();

var foreignObjectElement = createSVGElement("foreignObject");
foreignObjectElement.setAttribute("x", "100");
foreignObjectElement.setAttribute("y", "-80");
foreignObjectElement.setAttribute("width", "150");
foreignObjectElement.setAttribute("height", "150");

var htmlDivElement = document.createElementNS(xhtmlNS, "xhtml:div");
htmlDivElement.setAttribute("style", "background-color: green; color: white; text-align: center");
htmlDivElement.textContent = "Test passed";

foreignObjectElement.appendChild(htmlDivElement);
rootSVGElement.appendChild(foreignObjectElement);

shouldBeEqualToString("foreignObjectElement.getAttribute('y')", "-80");

function executeTest() {
    foreignObjectElement.setAttribute("y", "80");
    shouldBeEqualToString("foreignObjectElement.getAttribute('y')", "80");

    waitForClickEvent(foreignObjectElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
