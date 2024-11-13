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
testRegExp(/([\s\S]+?)Abc123([\s\S]+)EOL/, "1234  Abc123 5678 EOL", ["1234  Abc123 5678 EOL", "1234  ", " 5678 "]);
testRegExp(/([\s\S]+)ABC123([\s\S]+)EOL/i, "1234  Abc123 5678 EOL", ["1234  Abc123 5678 EOL", "1234  ", " 5678 "]);
testRegExp(/([\s\S]*?)Abc123([\s\S]+)EOL/, "1234  Abc123 5678 EOL", ["1234  Abc123 5678 EOL", "1234  ", " 5678 "]);
testRegExp(/([\s\S]*)ABC123([\s\S]+)EOL/i, "1234  Abc123 5678 EOL", ["1234  Abc123 5678 EOL", "1234  ", " 5678 "]);
testRegExp(/([\s\S]+?)Français([\s\S]+)EOL/, "1234  Français 5678 EOL", ["1234  Français 5678 EOL", "1234  ", " 5678 "]);

// Test 6
testRegExp(/([\s\S]+?)\u{03c0} = 3\.1415([\s\S]+)/u, "The value of \u{03c0} = 3.14159265358979", ["The value of \u{03c0} = 3.14159265358979", "The value of ", "9265358979"]);
testRegExp(/([\s\S]+?) Abc123([\s\S]+)EOL/u, "1234 \u{180e} Abc123 5678 EOL", ["1234 \u{180e} Abc123 5678 EOL", "1234 \u{180e}", " 5678 "]);
testRegExp(/([\s\S\u{180e}]+?) Abc123([\s\S]+)EOL/u, "1234 \u{180e} Abc123 5678 EOL", ["1234 \u{180e} Abc123 5678 EOL", "1234 \u{180e}", " 5678 "]);
testRegExp(/\S+/, " \u{180e} ", ["\u{180e}"]);
testRegExp(/[\S+X]/, " \u{180e} ", ["\u{180e}"]);

// Test 11
testRegExp(/([\s\S\q{Abc123}]+?)Abc123([\s\S]+)EOL/v, "1234  Abc123 5678 EOL", ["1234  Abc123 5678 EOL", "1234  ", " 5678 "]);
testRegExp(/([\s\S\q{Abc123}]+)Abc123([\s\S]+)EOL/v, "1234  Abc123 5678 EOL", ["1234  Abc123 5678 EOL", "1234  ", " 5678 "]);

// Test 13...22
// Create a character class of all lower and upper case characters in random order.
let r1 = /([vcWZtzagTVGHjshEKJUCYueMoQyAFPSqDwilIkBROpmfNnrxdbLX]*)[\s\.]/g;
let s1 = "The quick brown fox jumped over the lazy dogs back.";

testRegExp(r1, s1, ["The ", "The"]);
testRegExp(r1, s1, ["quick ", "quick"]);
testRegExp(r1, s1, ["brown ", "brown"]);
testRegExp(r1, s1, ["fox ", "fox"]);
testRegExp(r1, s1, ["jumped ", "jumped"]);
testRegExp(r1, s1, ["over ", "over"]);
testRegExp(r1, s1, ["the ", "the"]);
testRegExp(r1, s1, ["lazy ", "lazy"]);
testRegExp(r1, s1, ["dogs ", "dogs"]);
testRegExp(r1, s1, ["back.", "back"]);

