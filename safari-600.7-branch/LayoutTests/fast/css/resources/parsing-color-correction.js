description("This tests checks that all of the input values for -webkit-color-correction parse correctly.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue("-webkit-color-correction");
    document.body.removeChild(div);
    return result;
}

shouldBe('test("-webkit-color-correction: default;")', '"default"');
shouldBe('test("-webkit-color-correction: sRGB;")', '"srgb"');
shouldBe('test("-webkit-color-correction: srgb;")', '"srgb"');

shouldBe('test("-webkit-color-correction: apple;")', 'null');
shouldBe('test("-webkit-color-correction: 15;")', 'null');
shouldBe('test("-webkit-color-correction: auto;")', 'null');
