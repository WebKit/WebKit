// [Name] SVGTextElement-svgdom-rotate-prop.js
// [Expected rendering result] 'Test passed' message - and a series of PASS messages
description("Tests dynamic updates of the 'rotate' property of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "215");
textElement.setAttribute("rotate", "-90");
textElement.textContent = "Text content";
rootSVGElement.appendChild(textElement);

shouldBe("textElement.rotate.baseVal.getItem(0).value", "-90");

function repaintTest() {
    textElement.rotate.baseVal.getItem(0).value = 0;
    shouldBe("textElement.rotate.baseVal.getItem(0).value", "0");

    completeTest();
}

var successfullyParsed = true;
