// [Name] SVGCircleElement-dom-cy-attr.js
// [Expected rendering result] unclipped green circle - and a series of PASS messages

description("Tests dynamic updates of the 'cy' attribute of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "-50");
circleElement.setAttribute("r", "150");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBeEqualToString("circleElement.getAttribute('cy')", "-50");

function executeTest() {
    circleElement.setAttribute("cy", "150");
    shouldBeEqualToString("circleElement.getAttribute('cy')", "150");

    completeTest();
}

startTest(circleElement, 150, 50);

var successfullyParsed = true;
