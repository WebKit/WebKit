// [Name] SVGTextElement-svgdom-transform-prop.js
// [Expected rendering result] 'Test passed' message - and a series of PASS messages
description("Tests dynamic updates of the 'transform' property of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "215");
textElement.setAttribute("transform", "translate(0,0)");
textElement.textContent = "Text content";
rootSVGElement.appendChild(textElement);

shouldBe("textElement.transform.baseVal.getItem(0).matrix.f", "0.0");

function repaintTest() {
    textElement.transform.baseVal.getItem(0).matrix.f = -200;
    shouldBe("textElement.transform.baseVal.getItem(0).matrix.f", "-200.0");

    completeTest();
}

var successfullyParsed = true;
