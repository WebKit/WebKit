// With verbose set to false, this test is successful if there is no output.  Set verbose to true to see expected matches.
let verbose = false;

function arrayToString(arr)
{
    let str = '';
    arr.forEach(function(v, index) {
        if (typeof v == "string")
            str += "\"" + v + "\"";
        else
            str += v;

        if (index != (arr.length - 1))
            str += ',';
      });
  return str;
}

function objectToString(obj)
{
    let str = "";

    firstEntry = true;

    for (const [key, value] of Object.entries(obj)) {
        if (!firstEntry)
            str += ", ";

        str += key + ": " + dumpValue(value);

        firstEntry = false;
    }

    return "{ " + str + " }";
}

function dumpValue(v)
{
    if (v === null)
        return "<null>";

    if (v === undefined)
        return "<undefined>";

    if (typeof v == "string")
        return "\"" + v + "\"";

    let str = "";

    if (v.length)
        str += arrayToString(v);

    if (v.groups) {
        groupStr = objectToString(v.groups);

        if (str.length) {
            if ( groupStr.length)
                str += ", " + groupStr;
        } else
            str = groupStr;
    }

    return "[ " + str + " ]";
}

function compareArray(expected, actual)
{
    if (expected === null && actual === null)
        return true;

    if (expected === null) {
        print("### expected is null, actual is not null");
        return false;
    }

    if (actual === null) {
        print("### expected is not null, actual is null");
        return false;
    }

    if (expected.length !== actual.length) {
        print("### expected.length: " + expected.length + ", actual.length: " + actual.length);
        return false;
    }

    for (var i = 0; i < expected.length; i++) {
        if (expected[i] !== actual[i]) {
            print("### expected[" + i + "]: \"" + expected[i] + "\" !== actual[" + i + "]: \"" + actual[i] + "\"");
            return false;
        }
    }

    return true;
}

function compareGroups(expected, actual)
{
    if (expected === null && actual === null)
        return true;

    if (expected === null) {
        print("### expected group is null, actual group is not null");
        return false;
    }

    if (actual === null) {
        print("### expected group is not null, actual group is null");
        return false;
    }

    for (const key in expected) {
        if (expected[key] !== actual[key]) {
            print("### expected." + key + ": " + dumpValue(expected[key]) + " !== actual." + key + ": " + dumpValue(actual[key]));
            return false;
        }
    }

    return true;
}

let testNumber = 0;

function testRegExp(re, str, exp, groups)
{
    testNumber++;

    if (groups)
        exp.groups = groups;

    let actual = re.exec(str);

    let result = compareArray(exp, actual);;

    if (exp && exp.groups) {
        if (!compareGroups(exp.groups, actual.groups))
            result = false;
    }

    if (result) {
        if (verbose)
            print(re.toString() + ".exec(" + dumpValue(str) + "), passed ", dumpValue(exp));
    } else
        print(re.toString() + ".exec(" + dumpValue(str) + "), FAILED test #" + testNumber + ", Expected ", dumpValue(exp), " got ", dumpValue(actual));
}

function testRegExpSyntaxError(reString, flags, expError)
{
    testNumber++;


    try {
        let re = new RegExp(reString, flags);
        print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\", but it didn't");
    } catch (e) {
        if (e != expError)
            print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\" got \"" + e + "\"");
        else if (verbose)
            print("/" + reString + "/" + flags + " passed, it threw \"" + expError + "\" as expected");
    }
}

// Test 1
let re1 = /^(?:break|case|which|do|for)/i;

testRegExp(re1, "case", ["case"]);
testRegExp(re1, "FOR", ["FOR"]);
testRegExp(re1, "throw", null);

// Test 4
// ЛЕВЫЙ | ПРАВЫЙ | left | right  note: ЛЕВЫЙ is Russian for left and ПРАВЫЙ is Russian for right
let re2 = /^(?:\u{041b}\u{0415}\u{0412}\u{042b}\u{0419}|\u{041f}\u{0420}\u{0410}\u{0412}\u{042b}\u{0419}|left|right)$/u;

testRegExp(re2, "\u{041f}\u{0420}\u{0410}\u{0412}\u{042b}\u{0419}", ["\u{041f}\u{0420}\u{0410}\u{0412}\u{042b}\u{0419}"]);
testRegExp(re2, "\u{041b}\u{0415}\u{0412}\u{042b} ", null);
testRegExp(re2, "left", ["left"]);
testRegExp(re2, "center", null);

// Test 8
let re3 = /^(?:something|everything|anything)$/;
testRegExp(re3, "something", ["something"]);
testRegExp(re3, "anything", ["anything"]);
testRegExp(re3, "everything", ["everything"]);
testRegExp(re3, "anything but", null);

// Test 12
let re4 = /^(?:break|case|catch|continue|debugger|default|do|else|finally|for|function|if|return|switch|throw|try|var|while|with|null|true|false|instanceof|typeof|void|delete|new|in|this)/;

testRegExp(re4, "break", ["break"]);
testRegExp(re4, "catch", ["catch"]);
testRegExp(re4, "throw", ["throw"]);
testRegExp(re4, " throw", null);
testRegExp(re4, "return", ["return"]);
testRegExp(re4, "until", null);

// Test 18
let re5 = /^(?:break|case|catch|continue|debugger|default|do|else|finally|for|function|if|return|switch|throw|try|var|while|with|null|true|false|instanceof|typeof|void|delete|new|in|this)$/;

testRegExp(re5, "throw", ["throw"]);
testRegExp(re5, " throw", null);
testRegExp(re5, "function", ["function"]);
testRegExp(re5, "return", ["return"]);
testRegExp(re5, "let", null);
testRegExp(re5, "until", null);

// Test 24
let re6 = /^(?:a*|b*|c*)/;

testRegExp(re6, "", [""]);
testRegExp(re6, "a", ["a"]);
testRegExp(re6, "aa", ["aa"]);
testRegExp(re6, "b", [""]);
testRegExp(re6, "bbb", [""]);
testRegExp(re6, "c", [""]);
testRegExp(re6, "cc", [""]);

// Test 31
let re7 = /^(?:a+|b+|c+)/;

testRegExp(re7, "a", ["a"]);
testRegExp(re7, "aaa", ["aaa"]);
testRegExp(re7, "aa", ["aa"]);
testRegExp(re7, "b", ["b"]);
testRegExp(re7, "bb", ["bb"]);
testRegExp(re7, "bbb", ["bbb"]);
testRegExp(re7, "c", ["c"]);
testRegExp(re7, "cc", ["cc"]);
testRegExp(re7, "ccc", ["ccc"]);

// Test 40
let re8 = /^(?:a{1,}|b{1,}|c{1,})/;

testRegExp(re8, "a", ["a"]);
testRegExp(re8, "aa", ["aa"]);
testRegExp(re8, "aaa", ["aaa"]);
testRegExp(re8, "b", ["b"]);
testRegExp(re8, "bb", ["bb"]);
testRegExp(re8, "bbb", ["bbb"]);
testRegExp(re8, "c", ["c"]);
testRegExp(re8, "cc", ["cc"]);
testRegExp(re8, "ccc", ["ccc"]);

// Test 49
let re9 = /^(?:a{2}|b{2}|c{2})/;

testRegExp(re9, "a", null);
testRegExp(re9, "aa", ["aa"]);
testRegExp(re9, "aaa", ["aa"]);
testRegExp(re9, "b", null);
testRegExp(re9, "bb", ["bb"]);
testRegExp(re9, "bbb", ["bb"]);
testRegExp(re9, "c", null);
testRegExp(re9, "cc", ["cc"]);
testRegExp(re9, "ccc", ["cc"]);

// Test 58
let re10 = /^(?:a|aa|aaa)$/;
testRegExp(re10, "a", ["a"]);
testRegExp(re10, "aa", ["aa"]);
testRegExp(re10, "aaa", ["aaa"]);
testRegExp(re10, "aaaa", null);

let re11 = /^(?:aa|a|aaa)$/;
testRegExp(re11, "a", ["a"]);
testRegExp(re11, "aa", ["aa"]);
testRegExp(re11, "aaa", ["aaa"]);
testRegExp(re11, "aaaa", null);

let re12 = /^(?:aa|a|aaa)$/;
testRegExp(re12, "a", ["a"]);
testRegExp(re12, "aa", ["aa"]);
testRegExp(re12, "aaa", ["aaa"]);
testRegExp(re12, "aaaa", null);
