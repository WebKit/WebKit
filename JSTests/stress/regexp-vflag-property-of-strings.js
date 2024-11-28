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
testRegExpSyntaxError("[\\p{RGI_Emoji_Tag_Sequence}a]", "u", "SyntaxError: Invalid regular expression: invalid property expression");
testRegExpSyntaxError("\\P{Emoji_Keycap_Sequence}", "v", "SyntaxError: Invalid regular expression: negated class set may contain strings");
testRegExpSyntaxError("[a1&&\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z1&&\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a1-3&&\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 6
testRegExpSyntaxError("[a-z1-3&&\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z1-3&&\\p{White_Space}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a1--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z1--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a1-3--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 11
testRegExpSyntaxError("[a-z1-3--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z1-3--\\p{White_Space}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--x1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--\\q{k}1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z--\\q{k}1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 16
testRegExpSyntaxError("[\w--\\q{k}1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[\w--\\q{k}\\p{White_Space}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--x&&1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--\\q{k}&&1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z--\\q{k}&&1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 21
testRegExpSyntaxError("[\w--\\q{k}&&1]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[\w--\\q{k}&&\\p{White_Space}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&1\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z&&1\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[1-3&&a\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 26
testRegExpSyntaxError("[a-z&&1\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z&&1\\p{White_Space}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&1--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z&&1--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[1-3&&a--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 31
testRegExpSyntaxError("[a-z&&1--\\q{XYZ}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z&&1--\\p{White_Space}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&&\\p{White_Space}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&&]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&-]", "v", "SyntaxError: Invalid regular expression: invalid class set character");

// Test 36
testRegExpSyntaxError("[a&&(]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&)]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&[]", "v", "SyntaxError: Invalid regular expression: missing terminating ] for character class");
testRegExpSyntaxError("[a&&]]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&{]", "v", "SyntaxError: Invalid regular expression: invalid class set character");

// Test 41
testRegExpSyntaxError("[a&&}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&/]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&|]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&&&]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a&&!!]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 46
testRegExpSyntaxError("[a&&##]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&$$]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&%%]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&**]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&++]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 51
testRegExpSyntaxError("[a&&,,]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&..]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&::]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&;;]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&<<]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 56
testRegExpSyntaxError("[a&&==]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&>>]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&??]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&@@]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&^^]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 61
testRegExpSyntaxError("[a&&``]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&~~]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--(]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a--)]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a--[]", "v", "SyntaxError: Invalid regular expression: missing terminating ] for character class");

// Test 66
testRegExpSyntaxError("[a--]]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a--{]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a--}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a--/]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a--|]", "v", "SyntaxError: Invalid regular expression: invalid class set character");

// Test 71
testRegExpSyntaxError("[a--&&]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--!!]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--##]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--$$]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--%%]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 76
testRegExpSyntaxError("[a--**]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--++]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--,,]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--..]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--::]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 81
testRegExpSyntaxError("[a--;;]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--<<]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--==]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-->>]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--??]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 86
testRegExpSyntaxError("[a--@@]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--^^]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--``]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--~~]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a&&b-c]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 91
testRegExpSyntaxError("[a&&bc]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--b-c]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a--bc]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z--k]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a-z&&k]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 96
testRegExpSyntaxError("[a---]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a\\q{a&&b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a!!b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a##b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a$$b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 101
testRegExpSyntaxError("[a\\q{a%%b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a**b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a++b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a,,b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a..b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 106
testRegExpSyntaxError("[a\\q{a::b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a;;b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a<<b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a==b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a>>b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 111
testRegExpSyntaxError("[a\\q{a??b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a@@b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a^^b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a``b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[a\\q{a~~b}]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 116
testRegExpSyntaxError("[a\\q{a(b}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a\\q{a)b}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a\\q{a[b}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a\\q{a]b}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a\\q{a{b}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");

// Test 121
testRegExpSyntaxError("[a\\q{a/b}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[a\\q{a-b}]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[\p{ASCII}---]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[--]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[ab][--]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 126
testRegExpSyntaxError("[&&]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[ab][&&]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[++]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[ab][++]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[\\d\\q{x}-a]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 131
testRegExpSyntaxError("[\\p{ASCII}-a]", "v", "SyntaxError: Invalid regular expression: invalid range in character class for Unicode pattern");
testRegExpSyntaxError("[a--b]]", "v", "SyntaxError: Invalid regular expression: unmatched ] or } bracket for Unicode pattern");
testRegExpSyntaxError("[a&&b]]", "v", "SyntaxError: Invalid regular expression: unmatched ] or } bracket for Unicode pattern");
testRegExpSyntaxError("[\\p{Number}--]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[\\p{Number}&&]", "v", "SyntaxError: Invalid regular expression: invalid class set character");

// Test 136
testRegExpSyntaxError("[\\p{Number}--&&]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[\\p{Number}&&--]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[\\p{Number}----5]", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[\\p{Number}--&&5]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[\\p{Number}&&&&5]", "v", "SyntaxError: Invalid regular expression: invalid class set character");

// Test 141
testRegExpSyntaxError("[\\p{Number}&&--5]", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("[&", "v", "SyntaxError: Invalid regular expression: missing terminating ] for character class");
testRegExpSyntaxError("[-", "v", "SyntaxError: Invalid regular expression: missing terminating ] for character class");
testRegExpSyntaxError("[&&", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[--", "v", "SyntaxError: Invalid regular expression: invalid class set character");

// Test 146
testRegExpSyntaxError("[&&&", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[---", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[&-", "v", "SyntaxError: Invalid regular expression: missing terminating ] for character class");
testRegExpSyntaxError("[-&", "v", "SyntaxError: Invalid regular expression: invalid class set character");
testRegExpSyntaxError("[&&-", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");

// Test 151
testRegExpSyntaxError("[--&", "v", "SyntaxError: Invalid regular expression: invalid operation in class set");
testRegExpSyntaxError("a?[^\\q{ab}]", "v", "SyntaxError: Invalid regular expression: negated class set may contain strings");
testRegExpSyntaxError("a?[^\\q{ab}]?", "v", "SyntaxError: Invalid regular expression: negated class set may contain strings");
testRegExpSyntaxError("a?[^\\q{ab}]*", "v", "SyntaxError: Invalid regular expression: negated class set may contain strings");
testRegExpSyntaxError("a?[^\\q{ab}]+", "v", "SyntaxError: Invalid regular expression: negated class set may contain strings");

// Test 156
testRegExpSyntaxError("a?[^\\q{ab}]{1,3}", "v", "SyntaxError: Invalid regular expression: negated class set may contain strings");
testRegExp(/[\p{ASCII}&&\&]/v, "a&b", ["&"]);
testRegExp(/[\p{ASCII}&&\-]/v, "a-b", ["-"]);
testRegExp(/[\p{ASCII}--\&]/v, "&b", ["b"]);
testRegExp(/[\p{ASCII}--&]/v, "&b", ["b"]);

// Test 161
testRegExp(/[\p{ASCII}--\-]/v, "-b", ["b"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}a]/, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}a]/v, "a", ["a"]);
testRegExp(/(?:\u{1f3f4}\u{e0067}\u{e0062}\u{e0065}\u{e006e}\u{e0067}\u{e007F}|\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}|\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006C}\u{e0073}\u{e007F}|[a])/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}a]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);

// Test 166
testRegExp(/[a\p{RGI_Emoji_Tag_Sequence}]/v, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}}]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}}]/v, "a", null);
testRegExp(/[\q{\u{1f1fa}\u{1f1f8}}a]/v, "a", ["a"]);

// Test 171
testRegExp(/[\q{\u{1f1fa}\u{1f1f8}}a]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);
testRegExp(/[\q{\u{1f1fa}}\q{\u{1f1fa}\u{1f1f8}}]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);
testRegExp(/[\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}}\p{RGI_Emoji_Tag_Sequence}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}[\q{\u{1f1fa}\u{1f1f8}}a]]/v, "\u{1f1fa}\u{1f1f8}", ["\u{1f1fa}\u{1f1f8}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}[\q{\u{1f1fa}\u{1f1f8}}a]]/v, "a", ["a"]);

// Test 176
testRegExp(/[b-z[a]]/v, "a", ["a"]);
testRegExp(/[[a-z]--k]/v, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}--\q{\u{1F3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[a-z\q{X}]/v, "X", ["X"]);
testRegExp(/[[a-z]--\q{k}]/v, "a", ["a"]);

// Test 181
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}\q{\u{1f1fa}\u{1f1f8}|abc|a|\u{1f1fa}}]/v, "a", ["a"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}--\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", null);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}--\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}&&\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}", ["\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}"]);
testRegExp(/[\p{RGI_Emoji_Tag_Sequence}&&\q{\u{1f3f4}\u{e0067}\u{e0062}\u{e0073}\u{e0063}\u{e0074}\u{e007f}}]/v, "\u{1f3f4}\u{e0067}\u{e0062}\u{e0077}\u{e006c}\u{e0073}\u{e007f}", null);

// Test 186
testRegExp(/[[a-z]&&\q{a|e|i|o|u|X|Y|Z}]/v, "a", ["a"]);
testRegExp(/[\p{White_Space}&&\p{ASCII}]/v, " ", [" "]);
testRegExp(/[\p{White_Space}&&\p{ASCII}]/v, "\u2028", null);
testRegExp(/[\p{White_Space}--\p{ASCII}]/v, " ", null);
testRegExp(/[\p{White_Space}--\p{ASCII}]/v, "\u2028", ["\u2028"]);

// Test 191
testRegExp(/^[[0-9]&&\d]+$/v, "0", ["0"]);
testRegExp(/^[_--[0-9]]+$/v, "_", ["_"]);
testRegExp(/[[a-z]--[a]]/v, "a", null);
testRegExp(/^\p{RGI_Emoji_Flag_Sequence}+$/v, "\u{1F1E6}\u{1F1E8}", ["\u{1F1E6}\u{1F1E8}"]);
testRegExp(/^\p{RGI_Emoji_Flag_Sequence}+$/v, "\u{1F1E6}\u{1F1E8}\u{1f1e7}\u{1f1f1}", ["\u{1f1e6}\u{1f1e8}\u{1f1e7}\u{1f1f1}"]);

// Test 196
testRegExp(/^\p{RGI_Emoji_Flag_Sequence}+$/v, "\u{1f1f9}\u{1f1ef}\u{1F1E6}\u{1F1E8}\u{1f1f9}\u{1f1f7}", ["\u{1f1f9}\u{1f1ef}\u{1F1E6}\u{1F1E8}\u{1f1f9}\u{1f1f7}"]);
testRegExp(/^\p{Emoji_Keycap_Sequence}+$/v, "#\u{fe0f}\u{20e3}", ["#\u{fe0f}\u{20e3}"]);
testRegExp(/^\p{RGI_Emoji}+$/v, "#\u{fe0f}\u{20e3}", ["#\u{fe0f}\u{20e3}"]);
testRegExp(/[a\(\)]+/v, "a()", ["a()"]);
testRegExp(/[a\q{\(\)}]{2}/v, "()a()", ["()a"]);

// Test 201
testRegExp(/[a\q{\(\)}]+/v, "()a()", ["()a()"]);
testRegExp(/[a\q{\=\=}]+/v, "==a==", ["==a=="]);
testRegExp(/[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}a]/v, "a", ["a"]);
testRegExp(/[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}a]/v, "\u{0250}", ["\u{0250}"]);
testRegExp(/[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}a]/v, "\u{1813}", ["\u{1813}"]);

// Test 206
testRegExp(/[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}a]/v, "\u{3373}", ["\u{3373}"]);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}]]/v, "a", ["a"]);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}]]/v, "\u{0250}", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}]]/v, "\u{1813}", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}\q{\u{0250}}]]/v, "\u{3373}", null);

// Test 211
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}a]]/v, "a", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}a]]/v, "\u{0250}", ["\u{0250}"]);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}a]]/v, "\u{1813}", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{1813}}a]]/v, "\u{3373}", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{0250}}a]]/v, "a", null);

// Test 216
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{0250}}a]]/v, "\u{0250}", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{0250}}a]]/v, "\u{1813}", ["\u{1813}"]);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{3373}}\q{\u{0250}}a]]/v, "\u{3373}", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{1813}}\q{\u{0250}}a]]/v, "a", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{1813}}\q{\u{0250}}a]]/v, "\u{0250}", null);

// Test 221
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{1813}}\q{\u{0250}}a]]/v, "\u{1813}", null);
testRegExp(/[[\u{0250}-\u{3373}a]--[\q{\u{1813}}\q{\u{0250}}a]]/v, "\u{3373}", ["\u{3373}"]);
testRegExp(/[[\u{0250}-\u{3373}a]&&[a]]/v, "a", ["a"]);
testRegExp(/[[\u{0250}-\u{3373}a]&&[a]]/v, "\u{0250}", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[a]]/v, "\u{1813}", null);

// Test 226
testRegExp(/[[\u{0250}-\u{3373}a]&&[a]]/v, "\u{3373}", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{0250}}]]/v, "a", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{0250}}]]/v, "\u{0250}", ["\u{0250}"]);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{0250}}]]/v, "\u{1813}", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{0250}}]]/v, "\u{3373}", null);

// Test 231
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{1813}}]]/v, "a", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{1813}}]]/v, "\u{0250}", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{1813}}]]/v, "\u{1813}", ["\u{1813}"]);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{1813}}]]/v, "\u{3373}", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{3373}}]]/v, "a", null);

// Test 236
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{3373}}]]/v, "\u{0250}", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{3373}}]]/v, "\u{1813}", null);
testRegExp(/[[\u{0250}-\u{3373}a]&&[\q{\u{3373}}]]/v, "\u{3373}", ["\u{3373}"]);
testRegExp(/[[]a]/v, "a", ["a"]);
testRegExp(/[\p{ASCII}--[a-z]][\.!]/v, "TEST!", ["T!"]);

// Test 241
testRegExp(/[ab][c--c]/v, "a", null);
testRegExp(/a?[\q{bc}]?/v, "a", ["a"]);
testRegExp(/a?[\q{bc}]?/v, "ab", ["a"]);
testRegExp(/a?[\q{bc}]?/v, "abc", ["abc"]);
testRegExp(/a?[\q{bc}]?/v, "bc", ["bc"]);

// Test 246
testRegExp(/[a&&[\q{a|ab}]]/v, "ab", ["a"]);
testRegExp(/[a--[\q{ab}]]/v, "ab", ["a"]);
testRegExp(/[[\q{a|ab}]&&a]/v, "ab", ["a"]);
testRegExp(/[[\q{a|ab}]--a]/v, "ab", ["ab"]);
testRegExpSyntaxError("\\-", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 251
testRegExp(/[\q{}]*/v, "", [""]);
testRegExp(/[\q{}]+/v, "", [""]);
testRegExp(/[\q{}]*/v, "1234", [""]);
testRegExp(/[\q{|34||12}]*/v, "1234", ["1234"]);
testRegExp(/[\q{|34||12}]*/v, "3412", ["3412"]);
testRegExpSyntaxError("\\!", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\#", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\%", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\,", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\:", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 256
testRegExpSyntaxError("\\;", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\<", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\=", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\>", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\@", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 261
testRegExpSyntaxError("\\`", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\~", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\&]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\!]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\#]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 255
testRegExpSyntaxError("[\\%]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\,]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\:]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\;]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\<]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 271
testRegExpSyntaxError("[\\=]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\>]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\@]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\`]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("[\\~]", "u", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 276
testRegExpSyntaxError("\\&", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\-", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\!", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\#", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\%", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 281
testRegExpSyntaxError("\\,", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\:", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\;", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\<", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\=", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 286
testRegExpSyntaxError("\\>", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\@", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\`", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\~", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");
testRegExpSyntaxError("\\q{a}", "v", "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern");

// Test 291
testRegExp(/[\&\-\!\#\%\,\:\;\<\=\>\@\`\~]*/v, "&-!#%,:;<=>@`~", ["&-!#%,:;<=>@`~"]);
testRegExp(/[\q{\&\-\!\#\%\,\:\;\<\=\>\@\`\~}X]*/v, "X&-!#%,:;<=>@`~X", ["X&-!#%,:;<=>@`~X"]);
testRegExp(/[\q{}]/v, "", [""]);
testRegExp(/[\q{\u{0095}|k}]/vi, "k", ["k"]);
testRegExp(/[\q{\u{0095}|s}]/vi, "s", ["s"]);
