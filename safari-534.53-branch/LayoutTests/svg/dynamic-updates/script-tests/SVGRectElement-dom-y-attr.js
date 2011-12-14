// [Name] SVGRectElement-dom-y-attr.js
// [Expected rendering result] green rect size 200x200 at 0x0 - and a series of PASS messages

description("Tests dynamic updates of the 'y' attribute of the SVGRectElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "0");
rectElement.setAttribute("y", "-150");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("fill", "green");

rootSVGElement.appendChild(rectElement);
shouldBeEqualToString("rectElement.getAttribute('y')", "-150");

function executeTest() {
    rectElement.setAttribute("y", "0");
    shouldBeEqualToString("rectElement.getAttribute('y')", "0");

    completeTest();
}

startTest(rectElement, 100, 0);

var successfullyParsed = true;
