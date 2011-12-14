description("This tests checks that all of the input values for -webkit-font-smoothing parse correctly.");

function test(value)
{
    var span = document.createElement("span");
    span.setAttribute("style", value);
    document.body.appendChild(span);
    
    var result = span.style.getPropertyValue("-webkit-font-smoothing");
    document.body.removeChild(span);
    return result;
}

shouldBe('test("-webkit-font-smoothing: auto;")', '"auto"');
shouldBe('test("-webkit-font-smoothing: none;")', '"none"');
shouldBe('test("-webkit-font-smoothing: antialiased;")', '"antialiased"');
shouldBe('test("-webkit-font-smoothing: subpixel-antialiased;")', '"subpixel-antialiased"');

shouldBe('test("-webkit-font-smoothing: apple;")', 'null');
shouldBe('test("-webkit-font-smoothing: 15;")', 'null');
shouldBe('test("-webkit-font-smoothing: auto auto;")', 'null');

var successfullyParsed = true;
