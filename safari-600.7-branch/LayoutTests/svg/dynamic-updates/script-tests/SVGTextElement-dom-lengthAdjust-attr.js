// [Name] SVGTextElement-dom-lengthAdjust-attr.js
// [Expected rendering result] Text streteched using a skew transformation with a length of 200 - and a series of PASS messages

description("Tests dynamic updates of the 'lengthAdjust' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "215");
textElement.setAttribute("textLength", "200");
textElement.textContent = "Stretched text";
rootSVGElement.appendChild(textElement);

shouldBeNull("textElement.getAttribute('lengthAdjust')");
shouldBe("textElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACING");
shouldBe("textElement.textLength.baseVal.value", "200");
shouldBeTrue("lastLength = textElement.getComputedTextLength(); lastLength > 0 && lastLength < 200");

function repaintTest() {
    textElement.setAttribute("lengthAdjust", "spacingAndGlyphs");
    shouldBe("textElement.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");
    shouldBe("textElement.textLength.baseVal.value", "200");
    shouldBeTrue("textElement.getComputedTextLength() == lastLength");

    completeTest();
}

var successfullyParsed = true;
