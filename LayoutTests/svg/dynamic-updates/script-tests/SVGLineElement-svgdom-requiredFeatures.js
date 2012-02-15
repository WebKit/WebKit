// [Name] SVGLineElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var lineElement = createSVGElement("line");
lineElement.setAttribute("x1", "20");
lineElement.setAttribute("y1", "20");
lineElement.setAttribute("x2", "200");
lineElement.setAttribute("y2", "200");
lineElement.setAttribute("stroke", "green");
lineElement.setAttribute("stroke-width", "10px");

rootSVGElement.appendChild(lineElement);

function repaintTest() {
    description("Check that SVGLineElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "inline");
    description("Check that setting requiredFeatures to something invalid makes it not render");
    lineElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "");
    description("Check that setting requiredFeatures to something valid makes it render again");
    lineElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    lineElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    lineElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
