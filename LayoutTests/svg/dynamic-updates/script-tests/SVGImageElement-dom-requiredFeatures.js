// [Name] SVGImageElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var imageElement = createSVGElement("image");
imageElement.setAttribute("width", "200");
imageElement.setAttribute("height", "200");

rootSVGElement.appendChild(imageElement);

function repaintTest() {
    debug("Check that SVGImageElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(imageElement, null).display", "inline");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    imageElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(imageElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    imageElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(imageElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    imageElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(imageElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    imageElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(imageElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
