// [Name] SVGCircleElement-dom-cx-attr.js
// [Expected rendering result] unclipped green circle - and a series of PASS messages

description("Tests dynamic updates of the 'cx' attribute of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "-50");
circleElement.setAttribute("cy", "150");
circleElement.setAttribute("r", "150");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBeEqualToString("circleElement.getAttribute('cx')", "-50");

function executeTest() {
    circleElement.setAttribute("cx", "150");
    shouldBeEqualToString("circleElement.getAttribute('cx')", "150");

    completeTest();
}

startTest(circleElement, 50, 150);

var successfullyParsed = true;
