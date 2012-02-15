// [Name] SVGCircleElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("r", "200");

rootSVGElement.appendChild(circleElement);

function repaintTest() {
    debug("Check that SVGCircleElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(circleElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    circleElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(circleElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    circleElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldBeEqualToString("document.defaultView.getComputedStyle(circleElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    circleElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(circleElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    circleElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(circleElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
