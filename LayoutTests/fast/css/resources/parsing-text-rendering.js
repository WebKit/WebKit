description("This tests checks that all of the input values for background-repeat parse correctly.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue("text-rendering");
    document.body.removeChild(div);
    return result;
}

shouldBe('test("text-rendering: auto;")', '"auto"');
shouldBe('test("text-rendering: optimizeSpeed;")', '"optimizeSpeed"');
shouldBe('test("text-rendering: optimizeLegibility;")', '"optimizeLegibility"');
shouldBe('test("text-rendering: geometricPrecision;")', '"geometricPrecision"');

shouldBeEqualToString('test("text-rendering: auto auto;")', '');
shouldBeEqualToString('test("text-rendering: optimizeCoconuts;")', '');
shouldBeEqualToString('test("text-rendering: 15;")', '');
