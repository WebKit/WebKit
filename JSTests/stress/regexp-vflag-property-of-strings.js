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
    } catch (e) {
        if (e != expError)
            print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\" got \"" + e + "\"");
        else if (verbose)
            print("/" + reString + "/" + flags + " passed, it threw \"" + expError + "\" as expected");
    }
}

// Test 1
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}a]/, "a", ["a"]);
testRegExpSyntaxError("[\\p{RGI_Emoji_Tag_Sequence}a]", "u", "SyntaxError: Invalid regular expression: invalid property expression");
testRegExpSyntaxError("\\P{Emoji_Keycap_Sequence}", "v", "SyntaxError: Invalid regular expression: Negated class set may contain strings");
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}a]/v, "a", ["a"]);
testRegExp(/(?:\u{1f3f4}\u{e0067}\u{e0062}\u{e0065}\u{e006e}\u{e0067}\u{e007F}|\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}|\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006C}\u{e0073}\u{e007F}|[a])/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);

// Test 6
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}a]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}a]/v, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}}]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}}]/v, "a", null);

// Test 11
testRegExp(/[\q{\u{1f1fa}\u{1f1f8}}a]/v, "a", ["a"]);
testRegExp(/[\q{\u{1f1fa}\u{1f1f8}}a]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);
testRegExp(/[\q{\u{1f1fa}}\q{\u{1f1fa}\u{1f1f8}}]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);
testRegExp(/[\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}}\p{RGI_Emoji_Tag_Sequence}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}[\q{\u{1f1fa}\u{1f1f8}}a]]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);

// Test 16
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}[\q{\u{1f1fa}\u{1f1f8}}a]]/v, "a", ["a"]);
testRegExp(/[b-z[a]]/v, "a", ["a"]);
testRegExp(/[a-z--k]/v, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}--\q{\u{1F3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[a-z\q{X}]/v, "X", ["X"]);

// Test 21
testRegExp(/[a-z--\q{k}]/v, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}|abc|a|\u{1f1fa}}]/v, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}--\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", null);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}--\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}&&\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);

// Test 26
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}&&\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}", null);
testRegExp(/[a-z&&\q{a|e|i|o|u|X|Y|Z}]/v, "a", ["a"]);
testRegExp(/[\p{White_Space}&&\p{ASCII}]/v, " ", [" "]);
testRegExp(/[\p{White_Space}&&\p{ASCII}]/v, "\u2028", null);
testRegExp(/[\p{White_Space}--\p{ASCII}]/v, " ", null);

// Test 31
testRegExp(/[\p{White_Space}--\p{ASCII}]/v, "\u2028", ["\u2028"]);
testRegExp(/^[[0-9]&&\d]+$/v, "0", ["0"]);
testRegExp(/^[_--[0-9]]+$/v, "_", ["_"]);
testRegExp(/[[a-z]--[a]]/v, "a", null);
testRegExp(/^\p{RGI_Emoji_Flag_Sequence}+$/v, "\u{1F1E6}\u{1F1E8}", ["\u{1F1E6}\u{1F1E8}"]);

// Test 36
testRegExp(/^\p{RGI_Emoji_Flag_Sequence}+$/v, "\u{1F1E6}\u{1F1E8}\u{1f1e7}\u{1f1f1}", ["\u{1f1e6}\u{1f1e8}\u{1f1e7}\u{1f1f1}"]);
testRegExp(/^\p{RGI_Emoji_Flag_Sequence}+$/v, "\u{1f1f9}\u{1f1ef}\u{1F1E6}\u{1F1E8}\u{1f1f9}\u{1f1f7}", ["\u{1f1f9}\u{1f1ef}\u{1F1E6}\u{1F1E8}\u{1f1f9}\u{1f1f7}"]);
testRegExp(/^\p{Emoji_Keycap_Sequence}+$/v, "#\u{fe0f}\u{20e3}", ["#\u{fe0f}\u{20e3}"]);
testRegExp(/^\p{RGI_Emoji}+$/v, "#\u{fe0f}\u{20e3}", ["#\u{fe0f}\u{20e3}"]);
