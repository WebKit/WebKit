// [Name] SVGTextElement-dom-dx-attr.js
// [Expected rendering result] text message shifted to 0x20 - and a series of PASS message

description("Tests dynamic updates of the 'dx' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.textContent = "Text content";
textElement.setAttribute("x", "50");
textElement.setAttribute("y", "20");
textElement.setAttribute("dx", "0");
rootSVGElement.appendChild(textElement);

shouldBeEqualToString("textElement.getAttribute('dx')", "0");

function repaintTest() {
    textElement.setAttribute("dx", "-50");
    shouldBeEqualToString("textElement.getAttribute('dx')", "-50");
    completeTest();
}

var successfullyParsed = true;
