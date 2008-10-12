// [Name] SVGPatternElement-dom-patternTransform-attr.js
// [Expected rendering result] green ellipse, no red visible - and a series of PASS messages

description("Tests dynamic updates of the 'patternTransform' attribute of the SVGPatternElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "10");
rectElement.setAttribute("y", "10");
rectElement.setAttribute("width", "190");
rectElement.setAttribute("height", "190");
rectElement.setAttribute("fill", "url(#pattern)");

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var patternElement = createSVGElement("pattern");
patternElement.setAttribute("id", "pattern");
patternElement.setAttribute("width", "100");
patternElement.setAttribute("height", "50");
patternElement.setAttribute("patternUnits", "userSpaceOnUse");
patternElement.setAttribute("patternTransform", "matrix(1,0,0,1,0,0)");

var patternContentRect1Element = createSVGElement("rect");
patternContentRect1Element.setAttribute("width", "50");
patternContentRect1Element.setAttribute("height", "50");
patternContentRect1Element.setAttribute("fill", "green");
patternElement.appendChild(patternContentRect1Element);

var patternContentRect2Element = createSVGElement("rect");
patternContentRect2Element.setAttribute("x", "50");
patternContentRect2Element.setAttribute("width", "50");
patternContentRect2Element.setAttribute("height", "50");
patternContentRect2Element.setAttribute("fill", "red");
patternElement.appendChild(patternContentRect2Element);

defsElement.appendChild(patternElement);
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("patternElement.getAttribute('patternTransform')", "matrix(1,0,0,1,0,0)");

function executeTest() {
    patternElement.setAttribute("patternTransform", "matrix(4,0,0,1,0,0)");
    shouldBeEqualToString("patternElement.getAttribute('patternTransform')", "matrix(4,0,0,1,0,0)");

    completeTest();
}

startTest(rectElement, 150, 150);

var successfullyParsed = true;
