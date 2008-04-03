// [Name] SVGEllipseElement-dom-rx-attr.js
// [Expected rendering result] green ellipse - and a series of PASS mesages

description("Tests dynamic updates of the 'rx' attribute of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "0");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBeEqualToString("ellipseElement.getAttribute('rx')", "0");

function executeTest() {
    ellipseElement.setAttribute("rx", "100");
    shouldBeEqualToString("ellipseElement.getAttribute('rx')", "100");

    waitForClickEvent(ellipseElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
