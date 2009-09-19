// [Name] SVGEllipseElement-dom-cx-attr.js
// [Expected rendering result] unclipped green ellipse - and a series of PASS messages

description("Tests dynamic updates of the 'cx' attribute of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "-50");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBeEqualToString("ellipseElement.getAttribute('cx')", "-50");

function executeTest() {
    ellipseElement.setAttribute("cx", "150");
    shouldBeEqualToString("ellipseElement.getAttribute('cx')", "150");

    completeTest();
}

startTest(ellipseElement, 45, 150);

var successfullyParsed = true;
