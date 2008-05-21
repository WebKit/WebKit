description(
"This test checks whether arguments crashes when passed a bad index."
);

function indexArguments(index)
{
    return arguments[index];
}

shouldBe('indexArguments(1, "a")', '"a"');
shouldBe('indexArguments("1 ", "a")', 'undefined');
shouldBe('indexArguments(0xDEADBEEF)', 'undefined');
shouldBe('indexArguments(0xFFFFFFFF)', 'undefined');

var successfullyParsed = true;

