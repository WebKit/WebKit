// [Name] SVGGElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var gElement = createSVGElement("g");

rootSVGElement.appendChild(gElement);

function repaintTest() {
    debug("Check that SVGGElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(gElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    gElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(gElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    gElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(gElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    gElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(gElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    gElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(gElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
