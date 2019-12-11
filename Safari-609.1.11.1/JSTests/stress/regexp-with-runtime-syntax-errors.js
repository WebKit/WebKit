// This test checks that malformed regular expressions compiled at runtime throw SyntaxErrors

function testThrowsSyntaxtError(f)
{
    try {
        f();
    } catch (e) {
        if (!e instanceof SyntaxError)
            throw "Expected SynteaxError, but got: " + e;
    }
}

function fromExecWithBadUnicodeEscape()
{
    let baseRE = /\u{123x}/;
    let line = "abc";

    (new RegExp(baseRE, "u")).exec(line);
}

function fromTestWithBadUnicodeProperty()
{
    let baseRE = /a|\p{Blah}/;
    let line = "abc";

    (new RegExp(baseRE, "u")).test(line);
}

function fromSplitWithBadUnicodeIdentity()
{
    let baseRE = /,|:|\-/;
    let line = "abc:def-ghi";

    let fields = line.split(new RegExp(baseRE, "u"));
}

function fromMatchWithBadUnicodeBackReference()
{
    let baseRE = /\123/;
    let line = "xyz";

    let fields = line.match(new RegExp(baseRE, "u"));
}

function fromReplaceWithBadUnicodeEscape()
{
    let baseRE = /\%/;
    let line = "xyz";

    let fields = line.replace(new RegExp(baseRE, "u"), "x");
}

function fromSearchWithBadUnicodeEscape()
{
    let baseRE = /\=/;
    let line = "xyz";

    let fields = line.search(new RegExp(baseRE, "u"));
}

testThrowsSyntaxtError(fromExecWithBadUnicodeEscape);
testThrowsSyntaxtError(fromTestWithBadUnicodeProperty);
testThrowsSyntaxtError(fromSplitWithBadUnicodeIdentity);
testThrowsSyntaxtError(fromMatchWithBadUnicodeBackReference);
testThrowsSyntaxtError(fromReplaceWithBadUnicodeEscape);
testThrowsSyntaxtError(fromSearchWithBadUnicodeEscape);
