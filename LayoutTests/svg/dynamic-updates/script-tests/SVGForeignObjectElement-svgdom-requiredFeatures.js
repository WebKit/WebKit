// [Name] SVGForeignObjectElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var foreignObjectElement = createSVGElement("foreignObject");
foreignObjectElement.setAttribute("width", "200");
foreignObjectElement.setAttribute("height", "200");

var htmlDivElement = document.createElementNS(xhtmlNS, "xhtml:div");
htmlDivElement.setAttribute("style", "background-color: green; color: white; text-align: center");
htmlDivElement.textContent = "Test passed";

foreignObjectElement.appendChild(htmlDivElement);
rootSVGElement.appendChild(foreignObjectElement);

function repaintTest() {
    debug("Check that SVGForeignObjectElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(foreignObjectElement, null).display", "block");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    foreignObjectElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(foreignObjectElement, null).display", "");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    foreignObjectElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldBeEqualToString("document.defaultView.getComputedStyle(foreignObjectElement, null).display", "block");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    foreignObjectElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(foreignObjectElement, null).display", "block");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    foreignObjectElement.requiredFeatures.appendItem("foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(foreignObjectElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
