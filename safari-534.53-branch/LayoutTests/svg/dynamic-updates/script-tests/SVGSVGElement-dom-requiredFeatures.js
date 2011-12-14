// [Name] SVGSVGElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var svgElement = rootSVGElement;

function executeTest() {
    debug("Check that SVGSVGElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(svgElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    svgElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(svgElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    svgElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(svgElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    svgElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(svgElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    svgElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(svgElement, null).display", "");

    completeTest();
}

startTest(svgElement, 0, 100);

var successfullyParsed = true;
