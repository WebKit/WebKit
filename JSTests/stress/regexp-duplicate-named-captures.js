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

    let actual = re.exec(str);
    let result = compareArray(exp, actual);;

    if (exp) {
        if (groups && !compareGroups(groups, actual.groups))
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

    let threw = false;

    try {
        let re = new RegExp(reString, flags);
    } catch (e) {
        threw = true;
        if (e != expError)
            print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\" got \"" + e + "\"");
        else if (verbose)
            print("/" + reString + "/" + flags + " passed, it threw \"" + expError + "\" as expected");
    }

    if (!threw)
        print("FAILED test #" + testNumber + ", Expected /" + reString + "/" + flags + " to throw \"" + expError + "\", but it didn't");
}

// Test 1
testRegExp(/(?<A>123)|(?<A>456)/, "123", ["123", "123", undefined], { A: "123" });
testRegExp(/(?<A>123)|(?<A>456)/, "456", ["456", undefined, "456"], { A: "456" });
testRegExp(/(?:(?<mon>[1-9]|1[0-2])\/(?<day>[1-9]|[12][0-9]|3[01])\/(?<yr>19[6-9][0-9]|20[0-2][0-9]))|(?:(?<day>[1-9]|[12][0-9]|3[01])\-(?<mon>[1-9]|1[0-2])\-(?<yr>19[6-9][0-9]|20[0-2][0-9]))/, "3/24/1992", ["3/24/1992", "3", "24", "1992", undefined, undefined, undefined], { mon: "3", day: "24", yr: "1992" });
testRegExp(/(?:(?<mon>[1-9]|1[0-2])\/(?<day>[1-9]|[12][0-9]|3[01])\/(?<yr>19[6-9][0-9]|20[0-2][0-9]))|(?:(?<day>[1-9]|[12][0-9]|3[01])\-(?<mon>[1-9]|1[0-2])\-(?<yr>19[6-9][0-9]|20[0-2][0-9]))/, "24-3-1992", ["24-3-1992", undefined, undefined, undefined, "24", "3", "1992"], { mon: "3", day: "24", yr: "1992" });
testRegExp(/(?<x>a)|(?<x>b)/, "bab", ["b", undefined, "b"]);

// Test 6
testRegExp(/(?:(?<x>a)|(?<x>b))\k<x>/, "bb", ["bb", undefined, "b"], { x: "b"});
testRegExp(/(?:(?:(?<x>a)|(?<x>b))\k<x>){2}/, "aabb", ["aabb", undefined, "b"], { x: "b" });
testRegExp(/(?<A>abc)x|(?<A>abc)y|abcz/, "abcx", ["abcx", "abc", undefined], { A: "abc" });
testRegExp(/(?<A>abc)x|(?<A>abc)y|abcz/, "abcy", ["abcy", undefined, "abc"], { A: "abc" });
testRegExp(/(?<A>abc)x|(?<A>abc)y|abcz/, "abcz", ["abcz", undefined, undefined], { A: undefined });

// Test 11
testRegExp(/(?<A>abc)x|(?<B>abc)y|abcz/, "abcz", ["abcz", undefined, undefined], { A: undefined, B: undefined });
testRegExp(/(?<x>a)|(?<x>b)/, "ba", ["b", undefined, "b"], { x: "b"});
testRegExp(/(?<=(?:\k<x>{0}(?<x>a)))b/, "ab", ["b", "a"], { x: "a" });
testRegExp(/(?<=(?:\k<x>(?<x>c))|(?:\k<x>(?<x>a)))b/, "aab", ["b", undefined, "a"], { x: "a" });
testRegExp(/(?<=(?:\k<x>(?<x>c))|(?:\k<x>))b/, "aab", ["b", undefined], { x: undefined });
testRegExp(/(?<=(?:\k<x>(?<x>c))|(?:\k<y>(?<y>a)))b/, "aab", ["b", undefined, "a"], { x: undefined, y: "a" });
testRegExp(/(?<!(x)\1{0})b/, "xb", null);

// Test 18
testRegExp(/^(?:(?<a>x)|(?<a>y)|z)\k<a>$/, "z", ["z", undefined, undefined]);
testRegExp(/^(?:(?<a>x)|(?<a>y)|z){2}\k<a>$/, "xz", ["xz", undefined, undefined]);
testRegExp(/(?<a>x)|(?:zy\k<a>)/, "zy", ["zy", undefined]);
testRegExpSyntaxError("(?<A>123)(?<A>456)", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");
testRegExpSyntaxError("(?<A>123)(?:(?<A>456)|(?<A>789))", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");

// Test 23
testRegExpSyntaxError("(?<=\\k<a>a)x", "u", "SyntaxError: Invalid regular expression: invalid \\k<> named backreference");
testRegExp(/(?<b>.).\k<b>/u, "bab", ["bab", "b"]);
testRegExp(/(?<b>.).\k<b>/u, "baa", null, null);
testRegExp(/(?<a>\k<a>\w)../u, "bab", ["bab", "b"], { a: "b" });
testRegExp(/(?:(?<x>a)|(?<y>a)(?<x>b))(?:(?<z>c)|(?<z>d))/, "abc", ["abc", undefined, "a", "b", "c", undefined], { x: "b", y: "a", z: "c" });

// Test 28
testRegExp(/(?<=(?:\1|b)(aa))./, "aaaax", ["x", "aa"]);
testRegExp(/(?<=(?:\2|b)(?<=\1(a))(aa))./, "aaaax", ["x", "a", "aa"]);
testRegExp(/(?<=((?:\3|b))(?<=\2(a))(aa))./, "aaaax", ["x", "aa", "a", "aa"]);
testRegExpSyntaxError("((?:(?<f>\w))(?<f>.)(a*c)?)*", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");
//  &&&&  testRegExp(/((?:(?<f>\w))(?<f>.)(a*c)?)*/, "aabbbccc", ["aabbbcc","bcc","b","c","c"], { f: "c" });
testRegExp(/(?<A>)|(?<A>)*\k<A>/, "", ["", "", undefined], { A: "" });

// Test 33
testRegExp(/(?:(?<A>a)|(?<A>b)*)\k<A>/, "bb", ["bb",undefined,"b"], { A: "b" });
testRegExp(/((?<A>A+)(?<B>B+)(?<C>C+)(?<D>D+))+|((?<B>B+)(?<C>C+)(?<D>D+))+|((?<A>A+)(?<C>C+)(?<D>D+))+|((?<C>C+)(?<D>D+))+|((?<B>B+)(?<D>D+))+|((?<A>A+)(?<D>D+))+|((?<D>D+))/, "AAD", ["AAD", undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, "AAD", "AA", "D", undefined, undefined], { A: "AA", B: undefined, C: undefined, D: "D" });
testRegExpSyntaxError("(?<x>c)(?:(?<x>a)|(?<x>b))", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");
testRegExpSyntaxError("(?:(?<x>a)|(?<x>b))(?<x>c)", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");
testRegExpSyntaxError("(?:(?<x>c)x)(?:(?<x>a)|(?<x>b))", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");

// Test 38
testRegExpSyntaxError("(?:(?<x>a)|(?<x>b))(?:(?<x>c)x)", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");
testRegExpSyntaxError("(?:(?<x>c)x|(?<y>d))(?:(?<y>a)|(?<x>b))", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");
testRegExpSyntaxError("(?:(?<y>a)|(?<x>b))(?:(?<x>c)x|(?<y>d))", "", "SyntaxError: Invalid regular expression: duplicate group specifier name");
