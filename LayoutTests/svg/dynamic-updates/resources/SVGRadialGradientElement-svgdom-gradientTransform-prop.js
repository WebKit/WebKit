// [Name] SVGRadialGradientElement-svgdom-gradientTransform-prop.js
// [Expected rendering result] green ellipse, no red visible - and a series of PASS messages

description("Tests dynamic updates of the 'gradientTransform' property of the SVGRadialGradientElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "url(#gradient)");

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

var radialGradientElement = createSVGElement("radialGradient");
radialGradientElement.setAttribute("id", "gradient");
radialGradientElement.setAttribute("gradientTransform", "matrix(" + transform.matrix.a + " "
                                                                  + transform.matrix.b + " "
                                                                  + transform.matrix.c + " "
                                                                  + transform.matrix.d + " "
                                                                  + transform.matrix.e + " "
                                                                  + transform.matrix.f + ")");

var firstStopElement = createSVGElement("stop");
firstStopElement.setAttribute("offset", "0");
firstStopElement.setAttribute("stop-color", "red");
radialGradientElement.appendChild(firstStopElement);

var lastStopElement = createSVGElement("stop");
lastStopElement.setAttribute("offset", "1");
lastStopElement.setAttribute("stop-color", "green");
radialGradientElement.appendChild(lastStopElement);

defsElement.appendChild(radialGradientElement);
rootSVGElement.appendChild(ellipseElement);

shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.a", "1.0");
shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.b", "0.0");
shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.c", "0.0");
shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.d", "1.0");
shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.e", "0.0");
shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.f", "0.0");

function executeTest() {
    radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.e = 50;

    shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.a", "1.0");
    shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.b", "0.0");
    shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.c", "0.0");
    shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.d", "1.0");
    shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.e", "50.0");
    shouldBe("radialGradientElement.gradientTransform.baseVal.getItem(0).matrix.f", "0.0");

    completeTest();
}

startTest(ellipseElement, 150, 150);

var successfullyParsed = true;
