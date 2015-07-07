// [Name] SVGTextElement-dom-x-attr.js
// [Expected rendering result] Text message at 0x20 - and a series of PASS messages

description("Tests dynamic updates of the 'x' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "50");
textElement.setAttribute("y", "20");
textElement.textContent = "Text content";
rootSVGElement.appendChild(textElement);

shouldBeEqualToString("textElement.getAttribute('x')", "50");

function repaintTest() {
    textElement.setAttribute("x", "0");
    shouldBeEqualToString("textElement.getAttribute('x')", "0");

    completeTest();
}

var successfullyParsed = true;
