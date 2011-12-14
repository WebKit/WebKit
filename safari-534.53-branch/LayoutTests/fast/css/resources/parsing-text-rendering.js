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
shouldBe('test("text-rendering: optimizeSpeed;")', '"optimizespeed"');
shouldBe('test("text-rendering: optimizeLegibility;")', '"optimizelegibility"');
shouldBe('test("text-rendering: geometricPrecision;")', '"geometricprecision"');

shouldBeNull('test("text-rendering: auto auto;")');
shouldBeNull('test("text-rendering: optimizeCoconuts;")');
shouldBeNull('test("text-rendering: 15;")');

var successfullyParsed = true;
