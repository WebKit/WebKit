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

// Tests 1-10
let str = "\u21d2\u21d2\u21d2\u21d2\u21d2twoXtwoXXtwo\u{1f702}twoX\u{1f702}two\u{1f702}Xtwo\u{1f702}\u{1f702}twoX";
for (let i = 1; i <= 5; ++i) {
    let strTemp = str.slice(i);
    let re = /two/gu;
    testRegExp(re, strTemp, ["two"]);
    testRegExp(re, strTemp, ["two"]);
    testRegExp(re, strTemp, ["two"]);
    testRegExp(re, strTemp, ["two"]);
    testRegExp(re, strTemp, ["two"]);
    testRegExp(re, strTemp, ["two"]);
}

// Tests 31-40
str = "\u2192\u2192\u2192\u2192\u2192sevenXXseven\u2192\u2192";
for (let i = 1; i <= 5; ++i) {
    let strTemp = str.slice(i);
    let re = /seven/gu;
    testRegExp(re, strTemp, ["seven"]);
    testRegExp(re, strTemp, ["seven"]);
}

// Tests 41-50
str = "\u2192\u2192\u2192\u2192\u2192\u041e\u0434\u0438\u043dXX\u041e\u0434\u0438\u043d\u2192\u2192";
for (let i = 1; i <= 5; ++i) {
    let strTemp = str.slice(i);
    let re = /\u041e\u0434\u0438\u043d/gu;
    testRegExp(re, strTemp, ["\u041e\u0434\u0438\u043d"]);
    testRegExp(re, strTemp, ["\u041e\u0434\u0438\u043d"]);
}

// Tests 51-60
str = "\u2192\u2192\u2192\u2192\u2192\u0422\u0440\u0438XX\u0422\u0440\u0438\u2192\u2192";
for (let i = 1; i <= 5; ++i) {
    let strTemp = str.slice(i);
    let re = /\u0422\u0440\u0438/gu;
    testRegExp(re, strTemp, ["\u0422\u0440\u0438"]);
    testRegExp(re, strTemp, ["\u0422\u0440\u0438"]);
}

// Tests 61-70
str = "\u2192\u2192\u2192\u2192\u2192\u0427\u0435\u0442\u044b\u0440\u0435XX\u0427\u0435\u0442\u044b\u0440\u0435\u2192\u2192";
for (let i = 1; i <= 5; ++i) {
    let strTemp = str.slice(i);
    let re = /\u0427\u0435\u0442\u044b\u0440\u0435/gu;
    testRegExp(re, strTemp, ["\u0427\u0435\u0442\u044b\u0440\u0435"]);
    testRegExp(re, strTemp, ["\u0427\u0435\u0442\u044b\u0440\u0435"]);
}

// Tests 71-110
str = "\u2192\u2192\u2192\u2192\u2192one\u21c4\u041e\u0434\u0438\u043d\u2192three\u21c4\u0422\u0440\u0438\u21d2\u21d2seven\u21c4\u0427\u0435\u0442\u044b\u0440\u0435XX";
for (let i = 1; i <= 5; ++i) {
    let strTemp = str.slice(i);
    let re = /one|three|seven|\u041e\u0434\u0438\u043d|\u0422\u0440\u0438|\u0427\u0435\u0442\u044b\u0440\u0435/gu;
    testRegExp(re, strTemp, ["one"]);
    testRegExp(re, strTemp, ["\u041e\u0434\u0438\u043d"]);
    testRegExp(re, strTemp, ["three"]);
    testRegExp(re, strTemp, ["\u0422\u0440\u0438"]);
    testRegExp(re, strTemp, ["seven"]);
    testRegExp(re, strTemp, ["\u0427\u0435\u0442\u044b\u0440\u0435"]);
}
