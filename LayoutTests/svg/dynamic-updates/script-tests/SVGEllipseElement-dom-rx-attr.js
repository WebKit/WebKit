// [Name] SVGEllipseElement-dom-rx-attr.js
// [Expected rendering result] green ellipse with rx = ry (in fact a circle)- and a series of PASS messages

description("Tests dynamic updates of the 'rx' attribute of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "10");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBeEqualToString("ellipseElement.getAttribute('rx')", "10");

function executeTest() {
    ellipseElement.setAttribute("rx", "150");
    shouldBeEqualToString("ellipseElement.getAttribute('rx')", "150");

    completeTest();
}

startTest(ellipseElement, 150, 150);

var successfullyParsed = true;
