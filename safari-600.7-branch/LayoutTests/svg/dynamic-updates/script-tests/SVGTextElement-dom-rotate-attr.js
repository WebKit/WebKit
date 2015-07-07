// [Name] SVGTextElement-dom-x-attr.js
// [Expected rendering result] text message rotated by +20 degree  - and a series of PASS messages

description("Tests dynamic updates of the 'rotate' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "20");
textElement.setAttribute("rotate", "0");
textElement.textContent="Text content";

rootSVGElement.appendChild(textElement);
shouldBeEqualToString("textElement.getAttribute('rotate')", "0");

function repaintTest() {
    textElement.setAttribute("rotate", "20");
    shouldBeEqualToString("textElement.getAttribute('rotate')", "20");

    completeTest();
}

var successfullyParsed = true;
