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
        print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError);
    } catch (e) {
        if (e != expError)
            print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\" got \"" + e + "\"");
        else if (verbose)
            print("/" + reString + "/" + flags + " passed, it threw \"" + expError + "\" as expected");
    }
}

// Test 1
testRegExp(/\uDC9A$/u, "a\u{1F49A}", null);
testRegExp(/[\uDC9A|\uD83D]$/u, "a\u{1F49A}", null);
testRegExp(/[\uDC9A|\uD83D]+$/u, "a\u{1F49A}", null);
testRegExp(/[\uDC9A|\uD83D]+?$/u, "a\u{1F49A}", null);
testRegExp(/\uDC9A$/u, "a\uDC9A", ["\uDC9A"]);

// Test 6
testRegExp(/[\uDC9A|\uD83D]$/u, "a\uD83D", ["\uD83D"]);
testRegExp(/[\uDC9A|\uD83D]$/u, "a\uDC9A", ["\uDC9A"]);
testRegExp(/[\uDC9A|\uD83D]$/u, "a\uDC9A\uD83D", ["\uD83D"]);
testRegExp(/[\uDC9A|\uD83D]+$/u, "a\uD83D", ["\uD83D"]);
testRegExp(/[\uDC9A|\uD83D]+$/u, "a\uDC9A", ["\uDC9A"]);

// Test 11
testRegExp(/[\uDC9A|\uD83D]+$/u, "a\uDC9A\uD83D", ["\uDC9A\uD83D"]);
testRegExp(/[\uDC9A|\uD83D]+?$/u, "a\uD83D", ["\uD83D"]);
testRegExp(/[\uDC9A|\uD83D]+?$/u, "a\uDC9A", ["\uDC9A"]);
testRegExp(/[\uDC9A|\uD83D]+?$/u, "a\uDC9A\uD83D", ["\uDC9A\uD83D"]);
testRegExp(/[^\u{1F49A}]/u, "a\u{1F49A}", ["a"]);

// Test 16
testRegExp(/[^\u{1F49A}]/u, "\u{1F49A}a", ["a"]);
testRegExp(/(?<=(\u{1F49A}))a/u, "\u{1F49A}a", ["a", "\u{1F49A}"]);
testRegExp(/(?<=(\u{1F49A}))./u, "\u{1F49A}a", ["a", "\u{1F49A}"]);
testRegExp(/(?<=(\uDC9A))a/u, "\u{1F49A}a", null);
testRegExp(/(?<=(\uD83D))./u, "\u{1F49A}a", null);

// Test 21
testRegExp(/(?<=(\uDC9A))./u, "\u{1F49A}a", null);
