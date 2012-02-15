// [Name] SVGTextElement-svgdom-dx-prop.js
// [Expected rendering result] text message shifted to 0x20 - and a series of PASS message

description("Tests dynamic updates of the 'dx' property of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.textContent = "Text content";
textElement.setAttribute("x", "50");
textElement.setAttribute("y", "20");
textElement.setAttribute("dx", "0");
rootSVGElement.appendChild(textElement);

shouldBe("textElement.dx.baseVal.getItem(0).value", "0");

function repaintTest() {
    textElement.dx.baseVal.getItem(0).value = -50;
    shouldBe("textElement.dx.baseVal.getItem(0).value", "-50");
    completeTest();
}

var successfullyParsed = true;
