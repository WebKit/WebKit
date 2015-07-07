// [Name] SVGTextElement-dom-transform-attr.js
// [Expected rendering result] 'Test passed' message - and a series of PASS messages

description("Tests dynamic updates of the 'transform' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "215");
textElement.textContent = "Test passed";

rootSVGElement.appendChild(textElement);
shouldBeNull("textElement.getAttribute('transform')", "");

function repaintTest() {
    textElement.setAttribute("transform", "translate(0,-200)");
    shouldBeEqualToString("textElement.getAttribute('transform')", "translate(0,-200)");

    completeTest();
}

var successfullyParsed = true;
