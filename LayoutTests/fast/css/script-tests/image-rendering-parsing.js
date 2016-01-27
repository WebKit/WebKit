description("Test the parsing of the image-rendering property.");

var div;
function testImageRendering(value, computedValue)
{
    div = document.createElement("div");
    document.body.appendChild(div);
    div.setAttribute("style", "image-rendering: " + value);
    shouldBe("div.style.getPropertyCSSValue('image-rendering').cssValueType", 'CSSValue.CSS_PRIMITIVE_VALUE');
    shouldBeEqualToString("div.style.getPropertyValue('image-rendering')", value);
    shouldBeEqualToString("getComputedStyle(div).getPropertyValue('image-rendering')", computedValue);

    document.body.removeChild(div);
}

testImageRendering('auto', 'auto');
testImageRendering('crisp-edges', 'crisp-edges');
testImageRendering('pixelated', 'pixelated');
testImageRendering('-webkit-crisp-edges', 'crisp-edges');
testImageRendering('-webkit-optimize-contrast', 'crisp-edges');
testImageRendering('optimizespeed', 'optimizespeed');
testImageRendering('optimizequality', 'optimizequality');

successfullyParsed = true;
