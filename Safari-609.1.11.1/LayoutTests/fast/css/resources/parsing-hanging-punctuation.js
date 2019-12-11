description("This tests checks that all of the input values for hanging-punctuation parse correctly.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue("hanging-punctuation");
    document.body.removeChild(div);
    return result;
}

shouldBe('test("hanging-punctuation: none;")', '"none"');
shouldBe('test("hanging-punctuation: first;")', '"first"');
shouldBe('test("hanging-punctuation: last;")', '"last"');
shouldBe('test("hanging-punctuation: allow-end;")', '"allow-end"');
shouldBe('test("hanging-punctuation: force-end;")', '"force-end"');
shouldBe('test("hanging-punctuation: first last;")', '"first last"');
shouldBe('test("hanging-punctuation: last first;")', '"last first"');
shouldBe('test("hanging-punctuation: first allow-end;")', '"first allow-end"');
shouldBe('test("hanging-punctuation: first force-end;")', '"first force-end"');
shouldBe('test("hanging-punctuation: first allow-end last;")', '"first allow-end last"');
shouldBe('test("hanging-punctuation: last allow-end;")', '"last allow-end"');

shouldBeEqualToString('test("hanging-punctuation: first first;")', '');
shouldBeEqualToString('test("hanging-punctuation: nonsense;")', '');
shouldBeEqualToString('test("hanging-punctuation: allow-end force-end;")', '');
shouldBeEqualToString('test("hanging-punctuation: force-end allow-end;")', '');
shouldBeEqualToString('test("hanging-punctuation: last last;")', '');
shouldBeEqualToString('test("hanging-punctuation: 20px;")', '');
shouldBeEqualToString('test("hanging-punctuation: first allow-end force-end last;")', '');
