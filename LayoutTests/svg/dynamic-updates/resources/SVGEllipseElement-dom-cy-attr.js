// [Name] SVGEllipseElement-dom-cy-attr.js
// [Expected rendering result] green ellipse - and a series of PASS mesages

description("Tests dynamic updates of the 'cy' attribute of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "-150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBeEqualToString("ellipseElement.getAttribute('cy')", "-150");

function executeTest() {
    ellipseElement.setAttribute("cy", "150");
    shouldBeEqualToString("ellipseElement.getAttribute('cy')", "150");

    waitForClickEvent(ellipseElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
