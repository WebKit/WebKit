// This test verifies that a Unicode regular expression does not read past the end of a string.
// It should run without a crash or throwing an exception.

function testRegExpInbounds(re, str, substrEnd)
{
    let subStr = str.substring(0, substrEnd);

    let match = subStr.match(re);

    if (match !== null && match[0] === str) 
        throw "Error: Read past end of a Unicode substring processing a Unicode RegExp";
    else if (match === null || match[0] !== subStr) {
        print("Error: match[0].length = " + match[0].length + ", match[0] = \"" + match[0] + "\"");
        throw "Error: Didn't properly match a Unicode substring with a matching Unicode RegExp";
    }
}

testRegExpInbounds(/ab\u{10400}c\u{10a01}d|ab\u{10400}c\u{10a01}/iu, "ab\u{10428}c\u{10a01}d", 7);
testRegExpInbounds(/ab\u{10400}c\u{10a01}d|ab\u{10400}c\u{10a01}/iu, "ab\u{10428}c\u{10a01}d", 7);
testRegExpInbounds(/ab[\u{10428}x]c[\u{10a01}x]defg|ab\u{10428}c\u{10a01}def/u, "ab\u{10428}c\u{10a01}defg", 10);
testRegExpInbounds(/[\u{10428}x]abcd|\u{10428}abc/u, "\u{10428}abcdef", 5);
testRegExpInbounds(/ab\u{10400}c\u{10a01}[^d]|ab\u{10400}c\u{10a01}/iu, "ab\u{10428}c\u{10a01}X", 7);
testRegExpInbounds(/ab\u{10400}c\u{10a01}.|ab\u{10400}c\u{10a01}/iu, "ab\u{10428}c\u{10a01}d", 7);
testRegExpInbounds(/ab\u{10428}c\u{10a01}\u{10000}|ab\u{10428}c\u{10a01}/iu, "ab\u{10428}c\u{10a01}\u{10000}", 7);
testRegExpInbounds(/ab\u{10428}c\u{10a01}.|ab\u{10428}c\u{10a01}/u, "ab\u{10428}c\u{10a01}\u{10000}", 7);
testRegExpInbounds(/ab\u{10428}c\u{10a01}[^x]|ab\u{10428}c\u{10a01}/u, "ab\u{10428}c\u{10a01}\u{10000}", 7);
