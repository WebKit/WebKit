// [Name] SVGPatternElement-svgdom-patternTransform-prop.js
// [Expected rendering result] green ellipse, no red visible - and a series of PASS messages

description("Tests dynamic updates of the 'patternTransform' property of the SVGPatternElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "10");
rectElement.setAttribute("y", "10");
rectElement.setAttribute("width", "190");
rectElement.setAttribute("height", "190");
rectElement.setAttribute("fill", "url(#pattern)");

var transform = rootSVGElement.createSVGTransform();

shouldBe("transform.type", "SVGTransform.SVG_TRANSFORM_UNKNOWN");
shouldBe("transform.matrix.a", "1.0");
shouldBe("transform.matrix.b", "0.0");
shouldBe("transform.matrix.c", "0.0");
shouldBe("transform.matrix.d", "1.0");
shouldBe("transform.matrix.e", "0.0");
shouldBe("transform.matrix.f", "0.0");
shouldBe("transform.angle", "0.0");

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var patternElement = createSVGElement("pattern");
patternElement.setAttribute("id", "pattern");
patternElement.setAttribute("width", "100");
patternElement.setAttribute("height", "50");
patternElement.setAttribute("patternUnits", "userSpaceOnUse");
patternElement.setAttribute("patternTransform", "matrix(" + transform.matrix.a + " "
                                                          + transform.matrix.b + " "
                                                          + transform.matrix.c + " "
                                                          + transform.matrix.d + " "
                                                          + transform.matrix.e + " "
                                                          + transform.matrix.f + ")");


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

shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.a", "1.0");
shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.b", "0.0");
shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.c", "0.0");
shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.d", "1.0");
shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.e", "0.0");
shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.f", "0.0");

function executeTest() {
    patternElement.patternTransform.baseVal.getItem(0).matrix.a = 4;

    shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.a", "4.0");
    shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.b", "0.0");
    shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.c", "0.0");
    shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.d", "1.0");
    shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.e", "0.0");
    shouldBe("patternElement.patternTransform.baseVal.getItem(0).matrix.f", "0.0");

    completeTest();
}

startTest(rectElement, 150, 150);

var successfullyParsed = true;
