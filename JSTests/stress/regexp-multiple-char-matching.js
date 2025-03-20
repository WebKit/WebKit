// With verbose set to false, this test is successful if there is no output.  Set verbose to true to see expected matches.

let verbose = false;

let errors = 0;

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
            print("Test#" + testNumber + " " + re.toString() + ".exec(" + dumpValue(str) + "), passed ", dumpValue(exp));
    } else {
        print(re.toString() + ".exec(" + dumpValue(str) + "), FAILED test #" + testNumber + ", Expected ", dumpValue(exp), " got ", dumpValue(actual));
        errors++;
    }
}

function testRegExpSyntaxError(reString, flags, expError)
{
    testNumber++;


    try {
        let re = new RegExp(reString, flags);
        print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\", but it didn't");
        errors++;
    } catch (e) {
        if (e != expError)
            print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\" got \"" + e + "\"");
        else if (verbose)
            print("/" + reString + "/" + flags + " passed, it threw \"" + expError + "\" as expected");
    }
}

function printErrors()
{
    if (errors)
        throw "Test had " + errors + " errors";
}

// Tests 1-5
testRegExp(/ID\d+/, "ID123", ["ID123"]);
testRegExp(/abcde[12]f/, "abcde1f", ["abcde1f"]);
testRegExp(/a[12]b/, "a2b", ["a2b"]);
testRegExp(/c.lef/u, "c\u{1C345}lef", ["c\u{1C345}lef"]);
testRegExp(/\sbrown\s/, "The quick brown fox jumped.", [" brown "]);

// Tests 6-8
testRegExp(/h(?i:e)l(?i:l)o world/, "hello world", ["hello world"]);
testRegExp(/h(?i:e)l(?i:l)o world/, "Hello world", null);
testRegExp(/h(?i:e)l(?i:l)o world/, "hElLo world", ["hElLo world"]);

// Tests 9-41
let re = /^(?:break|case|catch|continue|debugger|default|do|else|finally|for|function|if|return|switch|throw|try|var|while|with|null|true|false|instanceof|typeof|void|delete|new|in|this)/;

testRegExp(re, "break", ["break"]);
testRegExp(re, "case", ["case"]);
testRegExp(re, "catch", ["catch"]);
testRegExp(re, "continue", ["continue"]);
testRegExp(re, "debugger", ["debugger"]);
testRegExp(re, "default", ["default"]);
testRegExp(re, "do", ["do"]);
testRegExp(re, "else", ["else"]);
testRegExp(re, "finally", ["finally"]);
testRegExp(re, "for", ["for"]);
testRegExp(re, "function", ["function"]);
testRegExp(re, "if", ["if"]);
testRegExp(re, "return", ["return"]);
testRegExp(re, "switch", ["switch"]);
testRegExp(re, "throw", ["throw"]);
testRegExp(re, "try", ["try"]);
testRegExp(re, "var", ["var"]);
testRegExp(re, "while", ["while"]);
testRegExp(re, "with", ["with"]);
testRegExp(re, "null", ["null"]);
testRegExp(re, "false", ["false"]);
testRegExp(re, "instanceof", ["instanceof"]);
testRegExp(re, "typeof", ["typeof"]);
testRegExp(re, "void", ["void"]);
testRegExp(re, "delete", ["delete"]);
testRegExp(re, "new", ["new"]);
testRegExp(re, "in", ["in"]);
testRegExp(re, "this", ["this"]);
testRegExp(re, "bre", null);
testRegExp(re, "c", null);
testRegExp(re, "cont", null);
testRegExp(re, " finally", null);
testRegExp(re, "v", null);

// Tests 42-50
// ЛЕВЫЙ | ПРАВЫЙ | ЦЕНТР | left | right | center
// note: ЛЕВЫЙ is Russian for left, ПРАВЫЙ is Russian for right and ЦЕНТР is Russian for center
let re2 = /\u{041b}\u{0415}\u{0412}\u{042b}\u{0419}|\u{041f}\u{0420}\u{0410}\u{0412}\u{042b}\u{0419}|\u{0426}\u{0415}\u{041d}\u{0422}\u{0420}|left|right|center/u;

testRegExp(re2, "\u{041b}\u{0415}\u{0412}\u{042b}\u{0419}", ["\u{041b}\u{0415}\u{0412}\u{042b}\u{0419}"]);
testRegExp(re2, "  \u{041f}\u{0420}\u{0410}\u{0412}\u{042b}\u{0419}", ["\u{041f}\u{0420}\u{0410}\u{0412}\u{042b}\u{0419}"]);
testRegExp(re2, "\u{0426}\u{0415}\u{041d}\u{0422}\u{0420}  ", ["\u{0426}\u{0415}\u{041d}\u{0422}\u{0420}"]);
testRegExp(re2, "\u{041b}\u{0415}\u{0412}\u{042b} ", null);
testRegExp(re2, "\u{0421}\u{0415}\u{0420}\u{0415}\u{0414}\u{0418}\u{041d}\u{0410}", null);
testRegExp(re2, " left", ["left"]);
testRegExp(re2, "  right  ", ["right"]);
testRegExp(re2, "in the center of the ring", ["center"]);
testRegExp(re2, "middle", null);

// Tests 51-55
testRegExp(/a\db\dc\d/, "a5b4c2", ["a5b4c2"]);
testRegExp(/a\db\dc\d/, "abc", null);
testRegExp(/a\db\dc\d/, "a5bc2", null);
testRegExp(/a\db\dc\d/, "ab1c2", null);
testRegExp(/a\db\dc\d/, "a3b1c", null);

// Tests 56-61
testRegExp(/a\db\dc\d/i, "a5b4c2", ["a5b4c2"]);
testRegExp(/a\db\dc\d/i, "A1B3C5", ["A1B3C5"]);
testRegExp(/a\db\dc\d/i, "A5b4c2", ["A5b4c2"]);
testRegExp(/a\db\dc\d/i, "a5B4c2", ["a5B4c2"]);
testRegExp(/a\db\dc\d/i, "a5b4C2", ["a5b4C2"]);
testRegExp(/a\db\dc\d/i, "aBc", null);

// Tests 62-67
testRegExp(/letters\d{12}numbers/, "letters123456789012numbers", ["letters123456789012numbers"]);
testRegExp(/letters\d{12}numbers/, "letters12345678901numbers", null);
testRegExp(/letters\d{12}numbers/, "letters1234567890123numbers", null);

testRegExp(/letters\d\d\d\d\d\d\d\d\d\d\d\dnumbers/, "letters123456789012numbers", ["letters123456789012numbers"]);
testRegExp(/letters\d\d\d\d\d\d\d\d\d\d\d\dnumbers/, "letters12345678901numbers", null);
testRegExp(/letters\d\d\d\d\d\d\d\d\d\d\d\dnumbers/, "letters1234567890123numbers", null);

// Tests 68-75
testRegExp(/ab\scd/, "ab cd", ["ab cd"]);
testRegExp(/ab\scd/, "ab\tcd", ["ab\tcd"]);
testRegExp(/ab\scd/, "abcd", null);
testRegExp(/ab\scd/, "ab  cd", null);
testRegExp(/ab\scd/, "AB CD", null);
testRegExp(/ab\scd/, "aB\tcD", null);
testRegExp(/ab\scd/, "ABCD", null);
testRegExp(/ab\scd/, "ab  Cd", null);

// Tests 76-83
testRegExp(/ab\scd/i, "AB CD", ["AB CD"]);
testRegExp(/ab\scd/i, "Ab\tCd", ["Ab\tCd"]);
testRegExp(/ab\scd/i, "ABCD", null);
testRegExp(/ab\scd/i, "ab  cd", null);
testRegExp(/ab\scd/i, "AB CD", ["AB CD"]);
testRegExp(/ab\scd/i, "aB\tcd", ["aB\tcd"]);
testRegExp(/ab\scd/i, "ABCD", null);
testRegExp(/ab\scd/i, "ab  Cd", null);

// Tests 84-91
testRegExp(/ab\s+cd/i, "AB CD", ["AB CD"]);
testRegExp(/ab\s+cd/i, "Ab\tCd", ["Ab\tCd"]);
testRegExp(/ab\s+cd/i, "ABCD", null);
testRegExp(/ab\s+cd/i, "ab  cd", ["ab  cd"]);
testRegExp(/ab\s+cd/i, "AB CD", ["AB CD"]);
testRegExp(/ab\s+cd/i, "aB\tcd", ["aB\tcd"]);
testRegExp(/ab\s+cd/i, "ABCD", null);
testRegExp(/ab\s+cd/i, "ab  Cd", ["ab  Cd"]);

// Tests 92-
class RegExpTest {
    constructor(re, expect) {
        this.re = re;
        this.expect = expect;
    }

    runTest(subjectString) {
        testRegExp(this.re, subjectString, this.expect);
    }
};

class RegExpTestList {
    #allTests;

    constructor() {
        this.#allTests = [];
    }

    addTest(re, expect) {
        this.#allTests.push(new RegExpTest(re, expect));
    }

    runTests(subjectString) {
        for (let test of this.#allTests)
            test.runTest(subjectString);
    }
};

let reTests = new RegExpTestList;

let ACharCode = "A".charCodeAt(0);
let aCharCode = "a".charCodeAt(0);
let MCharCode = "M".charCodeAt(0);
let mCharCode = "m".charCodeAt(0);
let spaces = "        ";
let maxSubStrLen = 12;

let strUpper = ""
let strLower = ""
let goodCharClasses = [""];
let badCharClasses = [""];

for (let len = 1; len <= spaces.length; ++len) {
    let goodCharClass = "";
    let badCharClass = "";
    for (let i = 0; i < len; ++i) {
        goodCharClass += "\\s";
        if (i == (len >> 1))
            badCharClass += "\\d";
        else
            badCharClass += "\\s";
    }
    goodCharClasses.push(goodCharClass);
    badCharClasses.push(badCharClass);
}

for (let len = 1; len < maxSubStrLen; ++len) {
    let subStrUpperPrefix = "";
    let subStrLowerPrefix = "";
    let subStrUpperSuffix = "";
    let subStrLowerSuffix = "";
    for (let charOffset = 0; charOffset < len; ++charOffset) {
        subStrUpperPrefix += String.fromCharCode(ACharCode + charOffset);
        subStrLowerPrefix += String.fromCharCode(aCharCode + charOffset);
        subStrUpperSuffix += String.fromCharCode(MCharCode + charOffset);
        subStrLowerSuffix += String.fromCharCode(mCharCode + charOffset);
    }


    for (let numSpaces = 0; numSpaces < spaces.length; ++numSpaces) {
        let subStrUpper = subStrUpperPrefix + spaces.slice(0, numSpaces) + subStrUpperSuffix;
        let subStrLower = subStrLowerPrefix + spaces.slice(0, numSpaces) + subStrLowerSuffix;

        reTests.addTest(new RegExp(subStrUpperPrefix + goodCharClasses[numSpaces] + subStrUpperSuffix), [subStrUpper]);
        reTests.addTest(new RegExp(subStrLowerPrefix + goodCharClasses[numSpaces] + subStrLowerSuffix), [subStrLower]);
        reTests.addTest(new RegExp(subStrUpperPrefix + goodCharClasses[numSpaces] + subStrLowerSuffix, "i"), [subStrUpper]);
        reTests.addTest(new RegExp(subStrLowerPrefix + goodCharClasses[numSpaces] + subStrUpperSuffix, "i"), [subStrUpper]);

        if (numSpaces) {
            reTests.addTest(new RegExp(subStrUpperPrefix + badCharClasses[numSpaces] + subStrUpperSuffix), null);
            reTests.addTest(new RegExp(subStrLowerPrefix + badCharClasses[numSpaces] + subStrLowerSuffix), null);
            reTests.addTest(new RegExp(subStrUpperPrefix + badCharClasses[numSpaces] + subStrLowerSuffix, "i"), null);
            reTests.addTest(new RegExp(subStrLowerPrefix + badCharClasses[numSpaces] + subStrUpperSuffix, "i"), null);

            let badLowerPrefix = "z";

            if (subStrLowerPrefix.length > 1){
                indexToChange = subStrLowerPrefix.length >> 1;
                badLowerPrefix = subStrLowerPrefix.slice(0, indexToChange) + "z" + subStrLowerPrefix.slice(indexToChange + 1);
            }
            reTests.addTest(new RegExp(badLowerPrefix + goodCharClasses[numSpaces] + subStrUpperSuffix), null);

            let badUpperSuffix = "z";

            if (subStrUpperSuffix.length > 1){
                indexToChange = subStrUpperSuffix.length >> 1;
                badUpperSuffix = subStrUpperSuffix.slice(0, indexToChange) + "z" + subStrUpperSuffix.slice(indexToChange + 1);
            }

            reTests.addTest(new RegExp(subStrLowerPrefix + goodCharClasses[numSpaces] + badUpperSuffix, "i"), null);
        }

        strUpper += subStrUpper + "-";
        strLower += subStrLower + "-";
    }
}

let str = "12345 " + strUpper + strLower;

reTests.runTests(str);

printErrors()
