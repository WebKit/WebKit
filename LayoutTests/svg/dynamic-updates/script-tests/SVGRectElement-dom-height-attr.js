// [Name] SVGRectElement-dom-height-attr.js
// [Expected rendering result] green rect size 200x200 at 0x0 - and a series of PASS messages

description("Tests dynamic updates of the 'height' attribute of the SVGRectElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "0");
rectElement.setAttribute("y", "0");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "50");
rectElement.setAttribute("fill", "green");

rootSVGElement.appendChild(rectElement);
shouldBeEqualToString("rectElement.getAttribute('height')", "50");

function executeTest() {
    rectElement.setAttribute("height", "200");
    shouldBeEqualToString("rectElement.getAttribute('height')", "200");

    completeTest();
}

startTest(rectElement, 100, 25);

var successfullyParsed = true;
