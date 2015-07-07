description("Tests the properties of the SVGException object.")

var e;
try {
    var svgDoc = document.implementation.createDocument("http://www.w3.org/2000/svg", "svg", null);
    var matrix = svgDoc.documentElement.createSVGMatrix();
    matrix.rotateFromVector(0, 0)
    // raises a SVG_INVALID_VALUE_ERR
} catch (err) {
    e = err;
}

shouldBeEqualToString("e.toString()", "Error: SVG_INVALID_VALUE_ERR: DOM SVG Exception 1");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object SVGException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object SVGExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "[object SVGExceptionConstructor]");
shouldBe("e.constructor", "window.SVGException");
shouldBe("e.SVG_WRONG_TYPE_ERR", "e.constructor.SVG_WRONG_TYPE_ERR");
shouldBe("e.SVG_WRONG_TYPE_ERR", "0");
shouldBe("e.SVG_INVALID_VALUE_ERR", "e.constructor.SVG_INVALID_VALUE_ERR");
shouldBe("e.SVG_INVALID_VALUE_ERR", "1");
shouldBe("e.SVG_MATRIX_NOT_INVERTABLE", "e.constructor.SVG_MATRIX_NOT_INVERTABLE");
shouldBe("e.SVG_MATRIX_NOT_INVERTABLE", "2");

var successfullyParsed = true;
