// [Name] SVGCircleElement-dom-r-attr.js
// [Expected rendering result] green circle - and a series of PASS mesages

description("Tests dynamic updates of the 'r' attribute of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "150");
circleElement.setAttribute("r", "0");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBeEqualToString("circleElement.getAttribute('r')", "0");

function executeTest() {
    circleElement.setAttribute("r", "150");
    shouldBeEqualToString("circleElement.getAttribute('r')", "150");

    waitForClickEvent(circleElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
