// [Name] SVGPolygonElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var polygonElement = createSVGElement("polygon");
polygonElement.setAttribute("points", "0,0 200,0 200,200 0, 200");

rootSVGElement.appendChild(polygonElement);

function repaintTest() {
    debug("Check that SVGPolygonElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polygonElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    polygonElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polygonElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    polygonElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polygonElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    polygonElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polygonElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    polygonElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(polygonElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
