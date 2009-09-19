// [Name] SVGEllipseElement-dom-cy-attr.js
// [Expected rendering result] unclipped green ellipse - and a series of PASS messages

description("Tests dynamic updates of the 'cy' attribute of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "-50");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBeEqualToString("ellipseElement.getAttribute('cy')", "-50");

function executeTest() {
    ellipseElement.setAttribute("cy", "150");
    shouldBeEqualToString("ellipseElement.getAttribute('cy')", "150");

    completeTest();
}

startTest(ellipseElement, 150, 50);

var successfullyParsed = true;
