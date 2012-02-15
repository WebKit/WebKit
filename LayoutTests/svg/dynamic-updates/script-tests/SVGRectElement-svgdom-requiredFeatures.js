// [Name] SVGRectElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");

rootSVGElement.appendChild(rectElement);

function repaintTest() {
    debug("Check that SVGRectElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    rectElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    rectElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    rectElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    rectElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(rectElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
