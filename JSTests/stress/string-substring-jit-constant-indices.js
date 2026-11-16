function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`Bad value: ${actual}!`);
}

function head(string)
{
    return string.substring(0, 5);
}
noInline(head);

function reversed(string)
{
    return string.substring(5, 1);
}
noInline(reversed);

function negative(string)
{
    return string.substring(-3, 2);
}
noInline(negative);

function huge(string)
{
    return string.substring(3, 1000);
}
noInline(huge);

function tail(string)
{
    return string.substring(4);
}
noInline(tail);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(head(""), "");
    shouldBe(reversed(""), "");
    shouldBe(negative(""), "");
    shouldBe(huge(""), "");
    shouldBe(tail(""), "");

    shouldBe(head("AB"), "AB");
    shouldBe(reversed("AB"), "B");
    shouldBe(negative("AB"), "AB");
    shouldBe(huge("AB"), "");
    shouldBe(tail("AB"), "");

    shouldBe(head("ABCDE"), "ABCDE");
    shouldBe(reversed("ABCDE"), "BCDE");
    shouldBe(negative("ABCDE"), "AB");
    shouldBe(huge("ABCDE"), "DE");
    shouldBe(tail("ABCDE"), "E");

    shouldBe(head("ABCDEFGHIJ"), "ABCDE");
    shouldBe(reversed("ABCDEFGHIJ"), "BCDE");
    shouldBe(negative("ABCDEFGHIJ"), "AB");
    shouldBe(huge("ABCDEFGHIJ"), "DEFGHIJ");
    shouldBe(tail("ABCDEFGHIJ"), "EFGHIJ");
}
