description("This tests that the methods on CSSPrimitiveValue throw exceptions ");

div = document.createElement('div');
div.style.width = "10px";
div.style.height = "90%";
div.style.content = "counter(dummy, square)";
div.style.clip = "rect(0, 0, 1, 1)";
div.style.color = "rgb(0, 0, 0)";

var invalidAccessError = "Error: InvalidAccessError: DOM Exception 15";

// Test passing invalid unit to getFloatValue
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_UNKNOWN)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_STRING)", "invalidAccessError");

// Test invalid unit conversions in getFloatValue
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_HZ)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_S)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_RAD)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_PERCENTAGE)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('height').getFloatValue(CSSPrimitiveValue.CSS_PX)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('height').getFloatValue(CSSPrimitiveValue.CSS_DEG)", "invalidAccessError");

// Test calling get*Value for CSSPrimitiveValue of the wrong type
shouldBe("div.style.getPropertyCSSValue('clip').primitiveType", "CSSPrimitiveValue.CSS_RECT");
shouldThrow("div.style.getPropertyCSSValue('clip').getFloatValue(CSSPrimitiveValue.CSS_PX)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('clip').getStringValue()", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('clip').getCounterValue()", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('clip').getRGBColorValue()", "invalidAccessError");

shouldBe("div.style.getPropertyCSSValue('color').primitiveType", "CSSPrimitiveValue.CSS_RGBCOLOR");
shouldThrow("div.style.getPropertyCSSValue('color').getFloatValue(CSSPrimitiveValue.CSS_PX)", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('color').getStringValue()", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('color').getCounterValue()", "invalidAccessError");
shouldThrow("div.style.getPropertyCSSValue('color').getRectValue()", "invalidAccessError");
