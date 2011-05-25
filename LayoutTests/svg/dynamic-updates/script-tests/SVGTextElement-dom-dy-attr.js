// [Name] SVGTextElement-dom-dy-attr.js
// [Expected rendering result] text message shifted to 0x20 - and a series of PASS messages

description("Tests dynamic updates of the 'dy' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "50");
textElement.setAttribute("dy", "0");
textElement.textContent="Text content";

rootSVGElement.appendChild(textElement);
shouldBeEqualToString("textElement.getAttribute('dy')", "0");

function executeTest() {
    textElement.setAttribute("dy", "-30");
    shouldBeEqualToString("textElement.getAttribute('dy')", "-30");

    completeTest();
}

startTest(textElement, 0, 50);

var successfullyParsed = true;
