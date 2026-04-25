description(
"Test non-escaping characters in string literals - added with https://bugs.webkit.org/show_bug.cgi?id=100580"
);

function test(character)
{
    shouldBeEqualToString('eval(\'"\\' + character + '"\')', character);
}

charactersToTest = " !#$%&\'()*+,-./:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`acdeghijklmopqswyz{|}~";

for (i = 0; i < charactersToTest.length; i++)
    test(charactersToTest.charAt(i));

function testOther(character)
{
    shouldEvaluateTo('eval(\'"\\' + character + '"\').charCodeAt(0)', character.charCodeAt(0));
}

// Test some characters outside the printable ASCII range
otherCharactersToTest = "\x01\x07\x08\x1f\xa0\xa3\xab\xb4";

for (i = 0; i < otherCharactersToTest.length; i++)
    testOther(otherCharactersToTest.charAt(i));
