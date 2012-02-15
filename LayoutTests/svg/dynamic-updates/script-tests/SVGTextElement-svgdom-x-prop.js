// [Name] SVGTextElement-svgdom-x-prop.js
// [Expected rendering result] Text message at 0x20 - and a series of PASS messages

description("Tests dynamic updates of the 'x' property of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "50");
textElement.setAttribute("y", "20");
textElement.textContent = "Text content";
rootSVGElement.appendChild(textElement);

shouldBe("textElement.x.baseVal.getItem(0).value", "50");

function repaintTest() {
    textElement.x.baseVal.getItem(0).value = 0;
    shouldBe("textElement.x.baseVal.getItem(0).value", "0");

    completeTest();
}

var successfullyParsed = true;
