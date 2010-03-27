description("This test ensures that setting primitive values to an inappropriate unit type will throw an exception.");

var element = document.createElement('div');
element.id = "test-element"
document.documentElement.appendChild(element);

var styleElement = document.createElement('style');
styleElement.innerText = "#test-element { left: 10px; font-family: Times; }";
document.documentElement.appendChild(styleElement);

var style = styleElement.sheet.cssRules[0].style;

var left = style.getPropertyCSSValue("left");

left.setFloatValue(CSSPrimitiveValue.CSS_NUMBER, 25);
shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "25");

left.setFloatValue(CSSPrimitiveValue.CSS_DIMENSION, 25);
shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_DIMENSION)", "25");

// Work around <http://webkit.org/b/31223> / <rdar://problem/7374538>.
left.setFloatValue(CSSPrimitiveValue.CSS_PX, 10);

shouldThrow("left.setFloatValue(CSSPrimitiveValue.CSS_UNKNOWN, 25)");
shouldThrow("left.setFloatValue(CSSPrimitiveValue.CSS_STRING, 25)");
shouldThrow("left.getFloatValue(CSSPrimitiveValue.CSS_UNKNOWN)");
shouldThrow("left.getFloatValue(CSSPrimitiveValue.CSS_STRING)");

shouldThrow("left.getStringValue()");
shouldThrow("left.getCounterValue()");
shouldThrow("left.getRectValue()");
shouldThrow("left.getRGBColorValue()");


var fontFamily = style.getPropertyCSSValue("font-family")[0];

fontFamily.setStringValue(CSSPrimitiveValue.CSS_STRING, "Hi there!");
shouldBe("fontFamily.getStringValue()", '"Hi there!"');

fontFamily.setStringValue(CSSPrimitiveValue.CSS_ATTR, "G'day!");
shouldBe("fontFamily.getStringValue()", '"G\'day!"');

shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_UNKNOWN, 'Hi there!')");
shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_DIMENSION, \"G'day!\")");
shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_COUNTER, 'Hello, world!')");

shouldThrow("fontFamily.getFloatValue()");
shouldThrow("fontFamily.getCounterValue()");
shouldThrow("fontFamily.getRectValue()");
shouldThrow("fontFamily.getRGBColorValue()");

successfullyParsed = true;
