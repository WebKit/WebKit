// [Name] SVGForeignObjectElement-dom-height-attr.js
// [Expected rendering result] 'Test passed' text - and a series of PASS mesages

description("Tests dynamic updates of the 'height' attribute of the SVGForeignObjectElement object")
createSVGTestCase();

var foreignObjectElement = createSVGElement("foreignObject");
foreignObjectElement.setAttribute("x", "100");
foreignObjectElement.setAttribute("y", "80");
foreignObjectElement.setAttribute("width", "150");
foreignObjectElement.setAttribute("height", "0");

var htmlDivElement = document.createElementNS(xhtmlNS, "xhtml:div");
htmlDivElement.setAttribute("style", "background-color: green; color: white; text-align: center");
htmlDivElement.textContent = "Test passed";

foreignObjectElement.appendChild(htmlDivElement);
rootSVGElement.appendChild(foreignObjectElement);

shouldBeEqualToString("foreignObjectElement.getAttribute('height')", "0");

function executeTest() {
    foreignObjectElement.setAttribute("height", "150");
    shouldBeEqualToString("foreignObjectElement.getAttribute('height')", "150");

    waitForClickEvent(foreignObjectElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
