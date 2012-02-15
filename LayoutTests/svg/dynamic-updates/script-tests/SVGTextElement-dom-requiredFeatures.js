// [Name] SVGTextElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var textElement = createSVGElement("text");

rootSVGElement.appendChild(textElement);

function repaintTest() {
    debug("Check that SVGTextElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(textElement, null).display", "block");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    textElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(textElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    textElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(textElement, null).display", "block");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    textElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(textElement, null).display", "block");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    textElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(textElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
