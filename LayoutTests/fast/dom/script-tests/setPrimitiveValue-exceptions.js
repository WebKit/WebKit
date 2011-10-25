description("Test exceptions thrown by the CSSPrimitiveValue APIs. Primitive values are currently immutable (see https://bugs.webkit.org/show_bug.cgi?id=31223)");

var element = document.createElement('div');
element.id = "test-element"
document.documentElement.appendChild(element);

var styleElement = document.createElement('style');
styleElement.innerText = "#test-element { position: absolute; left: 10px; font-family: Times; }";
document.documentElement.appendChild(styleElement);

function checkThrows(style) {
    left = style.getPropertyCSSValue("left");

    shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "10");
    shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_DIMENSION)", "10");
    shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_PX)", "10");

    shouldThrow("left.setFloatValue(CSSPrimitiveValue.CSS_NUMBER, 25)");
    shouldThrow("left.setFloatValue(CSSPrimitiveValue.CSS_DIMENSION, 25)");
    shouldThrow("left.setFloatValue(CSSPrimitiveValue.CSS_PX, 25)");
    shouldThrow("left.setFloatValue(CSSPrimitiveValue.CSS_UNKNOWN, 25)");
    shouldThrow("left.setFloatValue(CSSPrimitiveValue.CSS_STRING, 25)");
    shouldThrow("left.getFloatValue(CSSPrimitiveValue.CSS_UNKNOWN)");
    shouldThrow("left.getFloatValue(CSSPrimitiveValue.CSS_STRING)");

    shouldThrow("left.getStringValue()");
    shouldThrow("left.getCounterValue()");
    shouldThrow("left.getRectValue()");
    shouldThrow("left.getRGBColorValue()");

    shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "10");
    shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_DIMENSION)", "10");
    shouldBe("left.getFloatValue(CSSPrimitiveValue.CSS_PX)", "10");

    fontFamily = style.getPropertyCSSValue("font-family")[0];
    
    shouldBe("fontFamily.getStringValue()", '"Times"'); 

    shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_STRING, 'Hi there!')");
    shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_ATTR, \"G'day!\")");
    shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_UNKNOWN, 'Hi there!')");
    shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_DIMENSION, \"G'day!\")");
    shouldThrow("fontFamily.setStringValue(CSSPrimitiveValue.CSS_COUNTER, 'Hello, world!')");

    shouldThrow("fontFamily.getFloatValue()");
    shouldThrow("fontFamily.getCounterValue()");
    shouldThrow("fontFamily.getRectValue()");
    shouldThrow("fontFamily.getRGBColorValue()");
    
    shouldBe("fontFamily.getStringValue()", '"Times"');
}

var style = styleElement.sheet.cssRules[0].style;
checkThrows(style);

var computedStyle = window.getComputedStyle(element, null);
checkThrows(computedStyle);
