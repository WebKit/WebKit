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

// Test various valid combinations of flags set and unset
testRegExp(/(?i:[a-z])/, "A", ["A"]);
testRegExp(/(?i:[a-z])/i, "A", ["A"]);
testRegExp(/(?m:[a-z])/, "a", ["a"]);
testRegExp(/(?s:[a-z])/, "a", ["a"]);
testRegExp(/(?ims:[a-z])/, "A", ["A"]);
testRegExp(/(?is:[a-z])/, "A", ["A"]);
testRegExp(/(?si:[a-z])/, "A", ["A"]);
testRegExp(/(?i-m:[a-z])/, "A", ["A"]);
testRegExp(/(?i-ms:[a-z])/, "A", ["A"]);
testRegExp(/(?-ims:[a-z])/, "a", ["a"]);

// Test removing flags
testRegExp(/(?-i:[a-z])/i, "A", null);
testRegExp(/(?sm-i:[a-z])/i, "A", null);
testRegExp(/(?-s:[a-z])/is, "A", ["A"]);
testRegExp(/(?-ims:[a-z])/i, "A", null);

// Test adding and removing flags
testRegExp(/(?i:a(?-i:b)c)/, "AbC", ["AbC"]);
testRegExp(/(?i:a(?-i:b)c)/, "ABC", null);
testRegExp(/(?-i:a(?i:b)c)/i, "aBc", ["aBc"]);
testRegExp(/(?-i:a(?i:b)c)/i, "ABc", null);

testRegExp(/(?i:a(?m:b(?s:c(?-i:d)e)f)g)/i, "ABCdEFG", ["ABCdEFG"]);
testRegExp(/(?i:a(?m:b(?s:c(?-i:d)e)f)g)/i, "ABCDEFG", null);
testRegExp(/(?i:a(?m:b(?s:c(?-i:d.)e)f)g)/i, "ABCd\nEFG", ["ABCd\nEFG"]);

// Test any disjunction
testRegExp(/(?i:reallylong(regular)[expr])/, "ReallyLongRegularE", ["ReallyLongRegularE", "Regular"]);
testRegExp(/(?i-ms:reallylong(regular)[expr])/, "ReallyLongRegularE", ["ReallyLongRegularE", "Regular"]);

// Test nesting
testRegExp(/(?i:(?i:nest)ed)/, "nested", ["nested"]);
testRegExp(/(?i:(?i:nest)ed)/, "nestED", ["nestED"]);
testRegExp(/(?i:(?i-m:nest)ed)/, "nested", ["nested"]);
testRegExp(/(?i-m:(?i:nest)ed)/, "nested", ["nested"]);
testRegExp(/(?i-m:(?i-m:nest)ed)/, "nested", ["nested"]);

// Test incomplete parentheses group
testRegExpSyntaxError("(?i:[a-z]", "", "SyntaxError: Invalid regular expression: missing )");
testRegExpSyntaxError("(?i:", "", "SyntaxError: Invalid regular expression: missing )");
testRegExpSyntaxError("(?i", "", "SyntaxError: Invalid regular expression: missing )");

testRegExpSyntaxError("(?-i:[a-z]", "", "SyntaxError: Invalid regular expression: missing )");
testRegExpSyntaxError("(?-i:", "", "SyntaxError: Invalid regular expression: missing )");
testRegExpSyntaxError("(?-i", "", "SyntaxError: Invalid regular expression: missing )");
testRegExpSyntaxError("(?-", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");

testRegExpSyntaxError("(?m-i:[a-z]", "", "SyntaxError: Invalid regular expression: missing )");
testRegExpSyntaxError("(?m-i:", "", "SyntaxError: Invalid regular expression: missing )");
testRegExpSyntaxError("(?m-", "", "SyntaxError: Invalid regular expression: missing )");

// Test invalid flags
testRegExpSyntaxError("(?iu:[a-z])", "", "SyntaxError: Invalid regular expression: unrecognized character after (?");
testRegExpSyntaxError("(?u:[a-z])", "", "SyntaxError: Invalid regular expression: unrecognized character after (?");
testRegExpSyntaxError("(?u-i:[a-z])", "", "SyntaxError: Invalid regular expression: unrecognized character after (?");
testRegExpSyntaxError("(?i-u:[a-z])", "", "SyntaxError: Invalid regular expression: unrecognized character after (?");
testRegExpSyntaxError("(?-u:[a-z])", "", "SyntaxError: Invalid regular expression: unrecognized character after (?");

// It is a Syntax Error if the source text matched by RegularExpressionModifiers contains the same code point more than once.
testRegExpSyntaxError("(?ii:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");

// It is a Syntax Error if the source text matched by the first RegularExpressionModifiers and the source text matched by the second RegularExpressionModifiers are both empty.
testRegExpSyntaxError("(?-:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");

// It is a Syntax Error if the source text matched by the first RegularExpressionModifiers contains the same code point more than once.
testRegExpSyntaxError("(?ii-:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");
testRegExpSyntaxError("(?ii-m:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");
testRegExpSyntaxError("(?sis-m:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");

// It is a Syntax Error if the source text matched by the second RegularExpressionModifiers contains the same code point more than once.
testRegExpSyntaxError("(?i-mm:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");
testRegExpSyntaxError("(?-mim:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");
testRegExpSyntaxError("(?s-mimimimi:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");

// It is a Syntax Error if any code point in the source text matched by the first RegularExpressionModifiers is also contained in the source text matched by the second RegularExpressionModifiers.
testRegExpSyntaxError("(?i-i:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");
testRegExpSyntaxError("(?ism-i:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");
testRegExpSyntaxError("(?mis-sim:[a-z])", "", "SyntaxError: Invalid regular expression: invalid regular expression modifier");
