// [Name] SVGUseElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var useElement = createSVGElement("use");
var defsElement = createSVGElement("defs");
var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("id", "MyRect");
useElement.setAttributeNS(xlinkNS, "xlink:href", "#MyRect");

defsElement.appendChild(rectElement);
rootSVGElement.appendChild(defsElement);
rootSVGElement.appendChild(useElement);

function repaintTest() {
    description("Check that SVGUseElement is initially displayed");
    shouldBeEqualToString("document.defaultView.getComputedStyle(useElement, null).display", "inline");
    description("Check that setting requiredFeatures to something invalid makes it not render");
    useElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(useElement, null).display", "");
    description("Check that setting requiredFeatures to something valid makes it render again");
    useElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldBeEqualToString("document.defaultView.getComputedStyle(useElement, null).display", "inline");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    useElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeEqualToString("document.defaultView.getComputedStyle(useElement, null).display", "inline");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    useElement.setAttribute("requiredFeatures", "foo");
    shouldBeEqualToString("document.defaultView.getComputedStyle(useElement, null).display", "");

    completeTest();
}

var successfullyParsed = true;
