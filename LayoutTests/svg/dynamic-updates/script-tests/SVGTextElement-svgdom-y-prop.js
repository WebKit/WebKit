// [Name] SVGTextElement-svgdom-y-prop.js
// [Expected rendering result] text message at 0x20 - and a series of PASS messages

description("Tests dynamic updates of the 'y' property of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "50");
textElement.textContent="Text content";
rootSVGElement.appendChild(textElement);

shouldBe("textElement.y.baseVal.getItem(0).value", "50");

function repaintTest() {
    textElement.y.baseVal.getItem(0).value = 20;
    shouldBe("textElement.y.baseVal.getItem(0).value", "20");

    completeTest();
}

var successfullyParsed = true;
