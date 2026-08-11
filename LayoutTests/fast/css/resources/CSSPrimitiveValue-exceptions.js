description("This tests that the methods on CSSPrimitiveValue throw exceptions ");

div = document.createElement('div');
div.style.width = "10px";
div.style.height = "90%";
div.style.content = "counter(dummy, square)";
div.style.clip = "rect(0, 0, 1, 1)";
div.style.color = "rgb(0, 0, 0)";

// Test passing invalid unit to getFloatValue
shouldThrowErrorName("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_UNKNOWN)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_STRING)", "InvalidAccessError");

// Test invalid unit conversions in getFloatValue
shouldThrowErrorName("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_HZ)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_S)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_RAD)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_PERCENTAGE)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('height').getFloatValue(CSSPrimitiveValue.CSS_PX)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('height').getFloatValue(CSSPrimitiveValue.CSS_DEG)", "InvalidAccessError");

// Test calling get*Value for CSSPrimitiveValue of the wrong type
shouldBe("div.style.getPropertyCSSValue('clip').primitiveType", "CSSPrimitiveValue.CSS_RECT");
shouldThrowErrorName("div.style.getPropertyCSSValue('clip').getFloatValue(CSSPrimitiveValue.CSS_PX)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('clip').getStringValue()", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('clip').getCounterValue()", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('clip').getRGBColorValue()", "InvalidAccessError");

shouldBe("div.style.getPropertyCSSValue('color').primitiveType", "CSSPrimitiveValue.CSS_RGBCOLOR");
shouldThrowErrorName("div.style.getPropertyCSSValue('color').getFloatValue(CSSPrimitiveValue.CSS_PX)", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('color').getStringValue()", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('color').getCounterValue()", "InvalidAccessError");
shouldThrowErrorName("div.style.getPropertyCSSValue('color').getRectValue()", "InvalidAccessError");
