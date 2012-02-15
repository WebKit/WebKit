// [Name] SVGPolylineElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var polylineElement = createSVGElement("polyline");
polylineElement.setAttribute("points", "0,0 200,0 200,200 0, 200");

rootSVGElement.appendChild(polylineElement);

function repaintTest() {
    debug("Check that SVGPolylineElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polylineElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    polylineElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polylineElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    polylineElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polylineElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    polylineElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polylineElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    polylineElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polylineElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
