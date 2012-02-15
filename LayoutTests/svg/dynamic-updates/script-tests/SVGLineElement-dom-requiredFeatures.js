// [Name] SVGLineElement-dom-requiredFeatures.js
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
    debug("Check that SVGLineElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    lineElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    lineElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    lineElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    lineElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(lineElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
