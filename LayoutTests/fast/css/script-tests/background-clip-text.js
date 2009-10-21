description("This tests checks that the '-webkit-text' and 'text' keywords are \
    parsed correctly in the 'background-clip' and '-webkit-background-clip' \
    properties, and that 'background-clip' is parsed correctly in the \
    'background' shorthand.");

function test(declaration, property)
{
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue(property);
    document.body.removeChild(div);
    return result;
}

shouldBe('test("background-clip: -webkit-text", "background-clip")', '"-webkit-text"');
shouldBe('test("background-clip: -webkit-text", "-webkit-background-clip")', 'null');

shouldBe('test("background-clip: text", "background-clip")', 'null');
shouldBe('test("background-clip: text", "-webkit-background-clip")', 'null');

shouldBe('test("-webkit-background-clip: -webkit-text", "background-clip")', 'null');
shouldBe('test("-webkit-background-clip: -webkit-text", "-webkit-background-clip")', '"-webkit-text"');

shouldBe('test("-webkit-background-clip: text", "background-clip")', 'null');
shouldBe('test("-webkit-background-clip: text", "-webkit-background-clip")', '"text"');

shouldBe('test("background: url() padding-box", "background-clip")', '"padding-box"');
shouldBe('test("background: url() padding-box", "-webkit-background-clip")', 'null');

var successfullyParsed = true;
