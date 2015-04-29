description("Test of Unicode escape sequences in string literals and identifiers, especially code point escape sequences.");

function codeUnits(string)
{
    var result = [];
    for (var i = 0; i < string.length; ++i) {
        var hex = "000" + string.charCodeAt(i).toString(16).toUpperCase();
        result.push(hex.substring(hex.length - 4));
    }
    return result.join(",");
}

function testStringUnicodeEscapeSequence(sequence, expectedResult)
{
    shouldBeEqualToString('codeUnits("\\u' + sequence + '")', expectedResult);
}

function testInvalidStringUnicodeEscapeSequence(sequence)
{
    shouldThrow('codeUnits("\\u' + sequence + '")');
}

function testIdentifierStartUnicodeEscapeSequence(sequence, expectedResult)
{
    shouldBeEqualToString('codeUnits(function \\u' + sequence + '(){}.name)', expectedResult);
}

function testInvalidIdentifierStartUnicodeEscapeSequence(sequence)
{
    shouldThrow('codeUnits(function \\u' + sequence + '(){}.name)');
}

function testIdentifierPartUnicodeEscapeSequence(sequence, expectedResult)
{
    shouldBeEqualToString('codeUnits(function x\\u' + sequence + '(){}.name.substring(1))', expectedResult);
}

function testInvalidIdentifierPartUnicodeEscapeSequence(sequence)
{
    shouldThrow('codeUnits(function x\\u' + sequence + '(){}.name.substring(1))');
}

testStringUnicodeEscapeSequence("", "0075");
testStringUnicodeEscapeSequence("{0}", "0000");
testStringUnicodeEscapeSequence("{41}", "0041");
testStringUnicodeEscapeSequence("{D800}", "D800");
testStringUnicodeEscapeSequence("{d800}", "D800");
testStringUnicodeEscapeSequence("{DC00}", "DC00");
testStringUnicodeEscapeSequence("{dc00}", "DC00");
testStringUnicodeEscapeSequence("{FFFF}", "FFFF");
testStringUnicodeEscapeSequence("{ffff}", "FFFF");
testStringUnicodeEscapeSequence("{10000}", "D800,DC00");
testStringUnicodeEscapeSequence("{10001}", "D800,DC01");
testStringUnicodeEscapeSequence("{102C0}", "D800,DEC0");
testStringUnicodeEscapeSequence("{102c0}", "D800,DEC0");
testStringUnicodeEscapeSequence("{1D306}", "D834,DF06");
testStringUnicodeEscapeSequence("{1d306}", "D834,DF06");
testStringUnicodeEscapeSequence("{10FFFE}", "DBFF,DFFE");
testStringUnicodeEscapeSequence("{10fffe}", "DBFF,DFFE");
testStringUnicodeEscapeSequence("{10FFFF}", "DBFF,DFFF");
testStringUnicodeEscapeSequence("{10ffff}", "DBFF,DFFF");
testStringUnicodeEscapeSequence("{00000000000000000000000010FFFF}", "DBFF,DFFF");
testStringUnicodeEscapeSequence("{00000000000000000000000010ffff}", "DBFF,DFFF");

testInvalidStringUnicodeEscapeSequence("x");
testInvalidStringUnicodeEscapeSequence("{");
testInvalidStringUnicodeEscapeSequence("{}");
testInvalidStringUnicodeEscapeSequence("{G}");
testInvalidStringUnicodeEscapeSequence("{1G}");
testInvalidStringUnicodeEscapeSequence("{110000}");
testInvalidStringUnicodeEscapeSequence("{1000000}");
testInvalidStringUnicodeEscapeSequence("{100000000000000000000000}");

testIdentifierStartUnicodeEscapeSequence("{41}", "0041");
testIdentifierStartUnicodeEscapeSequence("{102C0}", "D800,DEC0");
testIdentifierStartUnicodeEscapeSequence("{102c0}", "D800,DEC0");
testIdentifierStartUnicodeEscapeSequence("{1D306}", "D834,DF06");
testIdentifierStartUnicodeEscapeSequence("{1d306}", "D834,DF06");

testInvalidIdentifierStartUnicodeEscapeSequence("");
testInvalidIdentifierStartUnicodeEscapeSequence("{0}");
testInvalidIdentifierStartUnicodeEscapeSequence("{D800}");
testInvalidIdentifierStartUnicodeEscapeSequence("{d800}");
testInvalidIdentifierStartUnicodeEscapeSequence("{DC00}");
testInvalidIdentifierStartUnicodeEscapeSequence("{dc00}");
testInvalidIdentifierStartUnicodeEscapeSequence("{FFFF}");
testInvalidIdentifierStartUnicodeEscapeSequence("{ffff}");
testInvalidIdentifierStartUnicodeEscapeSequence("{10000}");
testInvalidIdentifierStartUnicodeEscapeSequence("{10001}");
testInvalidIdentifierStartUnicodeEscapeSequence("{10FFFE}");
testInvalidIdentifierStartUnicodeEscapeSequence("{10fffe}");
testInvalidIdentifierStartUnicodeEscapeSequence("{10FFFF}");
testInvalidIdentifierStartUnicodeEscapeSequence("{10ffff}");
testInvalidIdentifierStartUnicodeEscapeSequence("{00000000000000000000000010FFFF}");
testInvalidIdentifierStartUnicodeEscapeSequence("{00000000000000000000000010ffff}");

testInvalidIdentifierStartUnicodeEscapeSequence("x");
testInvalidIdentifierStartUnicodeEscapeSequence("{");
testInvalidIdentifierStartUnicodeEscapeSequence("{}");
testInvalidIdentifierStartUnicodeEscapeSequence("{G}");
testInvalidIdentifierStartUnicodeEscapeSequence("{1G}");
testInvalidIdentifierStartUnicodeEscapeSequence("{110000}");
testInvalidIdentifierStartUnicodeEscapeSequence("{1000000}");
testInvalidIdentifierStartUnicodeEscapeSequence("{100000000000000000000000}");

testIdentifierPartUnicodeEscapeSequence("{41}", "0041");
testIdentifierPartUnicodeEscapeSequence("{10000}", "D800,DC00");
testIdentifierPartUnicodeEscapeSequence("{10001}", "D800,DC01");
testIdentifierPartUnicodeEscapeSequence("{102C0}", "D800,DEC0");
testIdentifierPartUnicodeEscapeSequence("{102c0}", "D800,DEC0");

testInvalidIdentifierPartUnicodeEscapeSequence("");
testInvalidIdentifierPartUnicodeEscapeSequence("{0}");
testInvalidIdentifierPartUnicodeEscapeSequence("{D800}");
testInvalidIdentifierPartUnicodeEscapeSequence("{d800}");
testInvalidIdentifierPartUnicodeEscapeSequence("{DC00}");
testInvalidIdentifierPartUnicodeEscapeSequence("{dc00}");
testInvalidIdentifierPartUnicodeEscapeSequence("{FFFF}");
testInvalidIdentifierPartUnicodeEscapeSequence("{ffff}");
testInvalidIdentifierPartUnicodeEscapeSequence("{1D306}");
testInvalidIdentifierPartUnicodeEscapeSequence("{1d306}");
testInvalidIdentifierPartUnicodeEscapeSequence("{10FFFE}");
testInvalidIdentifierPartUnicodeEscapeSequence("{10fffe}");
testInvalidIdentifierPartUnicodeEscapeSequence("{10FFFF}");
testInvalidIdentifierPartUnicodeEscapeSequence("{10ffff}");
testInvalidIdentifierPartUnicodeEscapeSequence("{00000000000000000000000010FFFF}");
testInvalidIdentifierPartUnicodeEscapeSequence("{00000000000000000000000010ffff}");

testInvalidIdentifierPartUnicodeEscapeSequence("x");
testInvalidIdentifierPartUnicodeEscapeSequence("{");
testInvalidIdentifierPartUnicodeEscapeSequence("{}");
testInvalidIdentifierPartUnicodeEscapeSequence("{G}");
testInvalidIdentifierPartUnicodeEscapeSequence("{1G}");
testInvalidIdentifierPartUnicodeEscapeSequence("{110000}");
testInvalidIdentifierPartUnicodeEscapeSequence("{1000000}");
testInvalidIdentifierPartUnicodeEscapeSequence("{100000000000000000000000}");

var successfullyParsed = true;
