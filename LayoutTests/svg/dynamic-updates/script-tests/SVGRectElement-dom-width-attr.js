// [Name] SVGRectElement-dom-width-attr.js
// [Expected rendering result] green rect size 200x200 at 0x0 - and a series of PASS messages

description("Tests dynamic updates of the 'width' attribute of the SVGRectElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "0");
rectElement.setAttribute("y", "0");
rectElement.setAttribute("width", "50");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("fill", "green");

rootSVGElement.appendChild(rectElement);
shouldBeEqualToString("rectElement.getAttribute('width')", "50");

function executeTest() {
    rectElement.setAttribute("width", "200");
    shouldBeEqualToString("rectElement.getAttribute('width')", "200");

    completeTest();
}

startTest(rectElement, 25, 100);

var successfullyParsed = true;
