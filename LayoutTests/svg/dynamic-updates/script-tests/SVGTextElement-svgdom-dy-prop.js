// [Name] SVGTextElement-svgdom-dy-prop.js
// [Expected rendering result] text message shifted to 0x20 - and a series of PASS messages

description("Tests dynamic updates of the 'dy' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "50");
textElement.setAttribute("dy", "0");
textElement.textContent="Text content";

rootSVGElement.appendChild(textElement);
shouldBe("textElement.dy.baseVal.getItem(0).value", "0");

function repaintTest() {
    textElement.dy.baseVal.getItem(0).value = -30;
    shouldBe("textElement.dy.baseVal.getItem(0).value", "-30");
    completeTest();
}

var successfullyParsed = true;
