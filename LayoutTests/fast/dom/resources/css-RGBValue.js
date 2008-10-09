description("This test ensures that objects RGBColor objects return the correct values.");

var element = document.createElement('div');
document.documentElement.appendChild(element);
element.style.color = "rgb(10, 20, 30)";

shouldBe("window.getComputedStyle(element, '').getPropertyCSSValue('color').getRGBColorValue().red.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "10");
shouldBe("window.getComputedStyle(element, '').getPropertyCSSValue('color').getRGBColorValue().green.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "20");
shouldBe("window.getComputedStyle(element, '').getPropertyCSSValue('color').getRGBColorValue().blue.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "30");

successfullyParsed = true;
