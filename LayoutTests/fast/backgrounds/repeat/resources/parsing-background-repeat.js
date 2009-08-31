description("This tests checks that all of the input values for background-repeat parse correctly.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue("background-repeat");
    document.body.removeChild(div);
    return result;
}

shouldBe('test("background-repeat: repeat-x;")', '"repeat-x"');
shouldBe('test("background-repeat: repeat-y;")', '"repeat-y"');
shouldBe('test("background-repeat: repeat;")', '"repeat"');
shouldBe('test("background-repeat: no-repeat;")', '"no-repeat"');
shouldBe('test("background-repeat: round;")', '"round"');
shouldBe('test("background-repeat: space;")', '"space"');

shouldBe('test("background-repeat: repeat repeat;")', '"repeat repeat"');
shouldBe('test("background-repeat: no-repeat space;")', '"no-repeat space"');
shouldBe('test("background-repeat: round round;")', '"round round"');
shouldBe('test("background-repeat: space repeat;")', '"space repeat"');

shouldBe('test("background: purple url(resources/gradient.gif) repeat-x top left")', '"repeat-x"');
shouldBe('test("background: purple url(resources/gradient.gif) repeat-y 50% 50%")', '"repeat-y"');
shouldBe('test("background: purple url(resources/gradient.gif) repeat center")', '"repeat"');
shouldBe('test("background: purple url(resources/gradient.gif) no-repeat 12px")', '"no-repeat"');
shouldBe('test("background: purple url(resources/gradient.gif) round 50 left")', '"round"');
shouldBe('test("background: purple url(resources/gradient.gif) space 25 25")', '"space"');

shouldBe('test("background-repeat: 45;")', 'null');
shouldBe('test("background-repeat: coconut;")', 'null');

var successfullyParsed = true;
