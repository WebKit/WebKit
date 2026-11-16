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

let re1 = /[\u{1f0a1}-\u{1f0ae}]{5}/u;

let stringsThatMatch1 = [
    "\u{1f0ae}\u{1f0a1}\u{1f0a2}\u{1f0a3}\u{1f0a4}",
    "\u{1f0a1}\u{1f0a2}\u{1f0a3}\u{1f0a4}\u{1f0a5}",
    "\u{1f0a3}\u{1f0a3}\u{1f0a4}\u{1f0a5}\u{1f0a6}",
    "\u{1f0a3}\u{1f0a4}\u{1f0a5}\u{1f0a6}\u{1f0a7}",
    "\u{1f0a3}\u{1f0a5}\u{1f0a6}\u{1f0a7}\u{1f0a8}",
    "\u{1f0a1}\u{1f0a3}\u{1f0a5}\u{1f0a7}\u{1f0a9}",
    "\u{1f0aa}\u{1f0a6}\u{1f0a4}\u{1f0a2}\u{1f0a8}",
    "\u{1f0aa}\u{1f0ae}\u{1f0ad}\u{1f0a1}\u{1f0a3}"];

for (i = 0; i < 500000; i++) {
    let str = stringsThatMatch1[i % stringsThatMatch1.length];

    testRegExp(re1, str, [str]);
}

let re2 = /(?:[\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}]{2,4})|(?:[\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}]{2,4})/u;

let stringsAndMatch2 = [
    ["\u{1f0a1}\u{1f0b1}\u{1f0d2}\u{1f0a3}\u{1f0a4}", "\u{1f0a1}\u{1f0b1}"],
    ["\u{1f0b1}\u{1f0ae}\u{1f0be}\u{1f0ce}\u{1f0d1}", "\u{1f0ae}\u{1f0be}\u{1f0ce}"],
    ["\u{1f0c1}\u{1f0b1}\u{1f0d1}\u{1f0a3}\u{1f0a4}", "\u{1f0c1}\u{1f0b1}\u{1f0d1}"],
    ["\u{1f0b1}\u{1f0ae}\u{1f0be}\u{1f0c1}\u{1f0d1}", "\u{1f0ae}\u{1f0be}"],
    ["\u{1f0a1}\u{1f0b3}\u{1f0d2}\u{1f0ae}\u{1f0ae}", "\u{1f0ae}\u{1f0ae}"],
    ["\u{1f0b1}\u{1f0ae}\u{1f0be}\u{1f0ce}\u{1f0d1}", "\u{1f0ae}\u{1f0be}\u{1f0ce}"],
    ["\u{1f0c1}\u{1f0b1}\u{1f0d1}\u{1f0a1}\u{1f0a4}", "\u{1f0c1}\u{1f0b1}\u{1f0d1}\u{1f0a1}"],
    ["\u{1f0b1}\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}", "\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}"]
];

for (i = 0; i < 500000; i++) {
    let strAndMatch = stringsAndMatch2[i % stringsAndMatch2.length];
    let str = strAndMatch[0];
    let match = strAndMatch[1];

    testRegExp(re2, str, [match]);
}
