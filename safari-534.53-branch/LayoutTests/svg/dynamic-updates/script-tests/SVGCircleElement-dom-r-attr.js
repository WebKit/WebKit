// [Name] SVGCircleElement-dom-r-attr.js
// [Expected rendering result] green circle - and a series of PASS messages

description("Tests dynamic updates of the 'r' attribute of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "150");
circleElement.setAttribute("r", "1");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBeEqualToString("circleElement.getAttribute('r')", "1");

function executeTest() {
    circleElement.setAttribute("r", "150");
    shouldBeEqualToString("circleElement.getAttribute('r')", "150");

    completeTest();
}

startTest(circleElement, 150, 150);

var successfullyParsed = true;
