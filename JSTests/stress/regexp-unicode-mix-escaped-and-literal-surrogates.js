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
            print(re.toString() + ".exec(" + dumpValue(str) + "), passed ", dumpValue(exp));
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

// Test a escaped surrogate paired with a literal surrogate.
testRegExp(new RegExp("\\ud800\udc00+", "u"), "\u{10000}\u{10000}", null);
testRegExp(new RegExp("^\\ud800\udc00+", "u"), "\u{10000}\u{10000}", null);
testRegExp(new RegExp("\\ud800\udc00+$", "u"), "\u{10000}\u{10000}", null);
testRegExp(new RegExp("^\\ud800\udc00+$", "u"), "\u{10000}\u{10000}", null);
testRegExp(new RegExp("\\uD83D\uDC38", "u"), "\u{1F438}", null);
testRegExp(new RegExp("\ud800\\udc00", "u"), "\u{10000}\u{10000}", null);
testRegExp(new RegExp("\uD83D\\uDC38", "u"), "\u{1F438}", null);
testRegExp(new RegExp("abcdefg\\ud800\udc001234567", "u"), "abcdefg\u{10000}1234567", null);
testRegExp(new RegExp("1234567\uD83D\\uDC38abcdefg", "u"), "1234567\u{1F438}abcdefg", null);

// Test well formed pairs of escaped surrogates
testRegExp(new RegExp("\\ud800\\udc00+", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("^\\ud800\\udc00+", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("\\ud800\\udc00+$", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("^\\ud800\\udc00+$", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("\\uD83D\\uDC38", "u"), "\u{1F438}", ["\u{1F438}"]);
testRegExp(new RegExp("abcdefg\\ud800\\udc001234567", "u"), "abcdefg\u{10000}1234567", ["abcdefg\u{10000}1234567"]);
testRegExp(new RegExp("1234567\\uD83D\\uDC38abcdefg", "u"), "1234567\u{1F438}abcdefg", ["1234567\u{1F438}abcdefg"]);

// Test well formed pairs of literal surrogates
testRegExp(new RegExp("\ud800\udc00+", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("^\ud800\udc00+", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("\ud800\udc00+$", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("^\ud800\udc00+$", "u"), "\u{10000}\u{10000}", ["\u{10000}\u{10000}"]);
testRegExp(new RegExp("\uD83D\uDC38", "u"), "\u{1F438}", ["\u{1F438}"]);
testRegExp(new RegExp("abcdefg\ud800\udc001234567", "u"), "abcdefg\u{10000}1234567", ["abcdefg\u{10000}1234567"]);
testRegExp(new RegExp("1234567\uD83D\uDC38abcdefg", "u"), "1234567\u{1F438}abcdefg", ["1234567\u{1F438}abcdefg"]);
