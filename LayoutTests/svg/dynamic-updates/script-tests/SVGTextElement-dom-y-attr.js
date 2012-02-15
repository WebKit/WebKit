// [Name] SVGTextElement-dom-y-attr.js
// [Expected rendering result] text message at 0x20 - and a series of PASS messages

description("Tests dynamic updates of the 'y' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "50");
textElement.textContent="Text content";
rootSVGElement.appendChild(textElement);

shouldBeEqualToString("textElement.getAttribute('y')", "50");

function repaintTest() {
    textElement.setAttribute("y", "20");
    shouldBeEqualToString("textElement.getAttribute('y')", "20");

    completeTest();
}

var successfullyParsed = true;
