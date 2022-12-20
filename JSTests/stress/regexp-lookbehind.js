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

    if (index != (arr.length - 1)) {
      str += ',';
    };
  });
  return "[" + str + "]";
}

function dumpValue(v)
{
    if (v === null)
        return "<null>";

    if (v === undefined)
        return "<undefined>";

    if (typeof v == "string")
        return "\"" + v + "\"";

    if (v.length)
        return arrayToString(v);

    return v;
}

function compareArray(a, b)
{
    if (a === null && b === null)
        return true;

    if (a === null) {
        print("### a is null, b is not null");
        return false;
    }

    if (b === null) {
        print("### a is not null, b is null");
        return false;
    }

    if (a.length !== b.length) {
        print("### a.length: " + a.length + ", b.length: " + b.length);
        return false;
    }

    for (var i = 0; i < a.length; i++) {
        if (a[i] !== b[i]) {
            print("### a[" + i + "]: \"" + a[i] + "\" !== b[" + i + "]: \"" + b[i] + "\"");
            return false;
        }
    }

    return true;
}

let testNumber = 0;

function testRegExp(re, str, exp)
{
    testNumber++;

    let actual = str.match(re);

    if (compareArray(exp, actual)) {
        if (verbose)
            print(dumpValue(str) +".match(" + re.toString() + "), passed ", dumpValue(exp));
    } else
        print(dumpValue(str) +".match(" + re.toString() + "), FAILED test #" + testNumber + ", Expected ", dumpValue(exp), " got ", dumpValue(actual));
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
            print("/" + reString + "/" + flags + "passed, it threw \"" + expError + "\" as expected");
    }
}

// Test 1
testRegExp(/(?<= )Dog/, " Dog", ["Dog"]);
testRegExp(/(?<=A )Dog/, "Walk A Dog", ["Dog"]);
testRegExp(/(?<=A )Dog/, "It's A Dog", ["Dog"]);
testRegExp(/((?<=A ))Dog/, "A Dog", ["Dog", ""]);
testRegExp(/((?<=A ))Dog/, "B Dog", null);

// Test 6
testRegExp(/(?<=(\w*) )Dog/, "Big Dog", ["Dog", "Big"]);
testRegExp(/(?<=(\w*) )Dog/, " B i g Dog", ["Dog", "g"]);
testRegExp(/Dog/, "Brown Dog", ["Dog"]);
testRegExp(/(?<=(\w{3}) )Dog/, "Brown Dog", ["Dog", "own"]);
testRegExp(/(?<=.*)Dog/, "Big Dog", ["Dog"]);

// Test 11
testRegExp(/(?<=(Big) )Dog/, "Big Dog", ["Dog", "Big"]);
testRegExp(/^.(?<=a)/, "a" , ["a"]);
testRegExp(/A (?<=A )Dog/, "A Dog", ["A Dog"]);  //  FAILS
testRegExp(/(?<=(b*))c/, "abbbbbbc", ["c", "bbbbbb"]);
testRegExp(/(?<=(b+))c/, "abbbbbbc", ["c", "bbbbbb"]);

// Test 16
testRegExp(/(?<=(b\d+))c/, "ab1234c", ["c", "b1234"]);
testRegExp(/(?<=((?:b\d{2})*))c/, "ab12b23b34c", ["c", "b12b23b34"]);
testRegExp(/(?<=((?:b\d{2})+))c/, "ab12b23b34c", ["c", "b12b23b34"]);
testRegExp(/.*(?<=(..|...|....))(.*)/, "xabcd", ["xabcd", "cd", ""]);
testRegExp(/.*(?<=(....|...|..))(.*)/, "xabcd", ["xabcd", "abcd", ""]);

// Test 21
testRegExp(/.*(?<=(xx|...|....))(.*)/, "xabcd", ["xabcd", "bcd", ""]);
testRegExp(/.*(?<=(xx|...))(.*)/, "xxabcd", ["xxabcd", "bcd", ""]);
testRegExp(/(?<=^abc)def/, "abcdef", ["def"]);
testRegExp(/(?<=([abc]+)).\1/, "abcdbc", null);
testRegExp(/(.)(?<=(\1\1))/, "abb", ["b", "b", "bb"]);

// Test 26
testRegExp(/(.)(?<=(\1\1))/i, "abB", ["B", "B", "bB"]);
testRegExp(/(?<=\1([abx]))d/, "abxxd", ["d", "x"]);
testRegExp(/((\w)\w)(?<=\1\2\1)/i, "aabAaBa", ["aB", "aB", "a"]);
testRegExp(/(?<=\1(\w+))c/, "ababc", ["c", "ab"]);
testRegExp(/(?<=\1(\w+))c/, "ababbc", ["c", "b"]);

// Test 31
testRegExp(/(?<=\1(\w+))c/, "ababdc", null);
testRegExp(/(?<=(\w+)\1)c/, "ababc", ["c", "abab"]);
testRegExp(/(?<=(\w){3})def/, "abcdef", ["def", "a"]);
testRegExp(/(?<=\b)[d-f]{3}/,"abc def", ["def"]);
testRegExp(/(?<=\B)\w{3}/, "ab cdef" , ["def"]);

// Test 36
testRegExp(/(?<=\B)(?<=c(?<=\w))\w{3}/, "ab cdef", ["def"]);
testRegExp(/(?<=\b)[d-f]{3}/, "abcdef", null);
testRegExp(/(?<!abc)\w\w\w/, "abcdef", ["abc"]);
testRegExp(/(?<!a.c)\w\w\w/, "abcdef", ["abc"]);
testRegExp(/(?<!a\wc)\w\w\w/, "abcdef", ["abc"]);

// Test 41
testRegExp(/(?<!a[a-z])\w\w\w/, "abcdef", ["abc"]);
testRegExp(/(?<!a[a-z]{2})\w\w\w/, "abcdef", ["abc"]);
testRegExp(/(?<!abc)def/, "abcdef", null);
testRegExp(/(?<!a.c)def/, "abcdef", null);
testRegExp(/(?<=ab\wd)\w\w/, "abcdef", ["ef"]);

// Test 46
testRegExp(/(?<=ab(?=c)\wd)\w\w/, "abcdef", ["ef"]);
testRegExp(/(?<=a\w{3})\w\w/, "abcdef", ["ef"]);
testRegExp(/(?<=a(?=([^a]{2})d)\w{3})\w\w/, "abcdef", ["ef", "bc"]);
testRegExp(/(?<=a(?=([bc]{2}(?<!a{2}))d)\w{3})\w\w/, "abcdef", ["ef", "bc"]);
testRegExp(/^faaao?(?<=^f[oa]+(?=o))/, "faaao", ["faaa"]);

// Test 51
testRegExp(/(?<=a(?=([bc]{2}(?<!a*))d)\w{3})\w\w/, "abcdef", null);
testRegExp(/(?<!abc)\w\w\w/, "abcdef", ["abc"]);
testRegExp(/(?<=^[^a-c]{3})def/, "abcdef", null);
testRegExp(/^(f)oo(?<=^\1o+)$/i, "foo", ["foo", "f"]);
testRegExp(/\w+(?<=$)/gm, "ab\ncd\nefg", ["ab", "cd", "efg"]);

// Test 56
testRegExp(/(?<=(bb*))c/, "abbbbbbc", ["c", "bbbbbb"]);
testRegExp(/(?<=(b*?))c/, "abbbbbbc", ["c", ""]);
testRegExp(/(?<=(b*?))c/, "abbbbbbc", ["c", ""]);
testRegExp(/(?<=(ab*?))c/, "abbbbbbc", ["c", "abbbbbb"]);
testRegExp(/(?<=(bb*?))c/, "abbbbbbc", ["c", "b"]);

// Test 61
testRegExp(/(?<=(b*?))c/i, "abbbbbbc", ["c", ""]);
testRegExp(/(?<=(b*?))c/i, "abbbbbbc", ["c", ""]);
testRegExp(/(?<=(ab*?))c/i, "abbbbbbc", ["c", "abbbbbb"]);
testRegExp(/(?<=(bb*?))c/i, "abbbbbbc", ["c", "b"]);
testRegExp(/(?<=(\u{10000}|\u{10400}|\u{10429}))x/u, "\u{10400}x", ["x", "\u{10400}"]);

// Test 66
testRegExp(/(?<=(\u{10000}|\u{10400}{2}|\u{10429}))x/u, "\u{10400}x", null);
testRegExp(/(?<=(\u{10000}|\u{10400}|\u{10429}))x/ui, "\u{10400}x", ["x", "\u{10400}"]);
testRegExp(/(?<=([^\u{10000}\u{10429}]))x/u, "\u{10400}x", ["x", "\u{10400}"]);
testRegExp(/(?<=([^\u{10000}\u{10429}]))x/ui, "\u{10400}x", ["x", "\u{10400}"]);
testRegExp(/(?<=([^\u{10000}\u{10429}]*))x/u, "\u{10400}\u{10406}x", ["x", "\u{10400}\u{10406}"]);

// Test 71
testRegExp(/(?<=([^\u{10000}\u{10429}]*))x/ui, "\u{10400}\u{10406}x", ["x", "\u{10400}\u{10406}"]);
testRegExp(/(?<=([^\u{10000}\u{10429}]*))x/u, "\u{10401}\u{10400}\u{10406}x", ["x", "\u{10401}\u{10400}\u{10406}"]);
testRegExp(/(?<=([^\u{10000}\u{10429}]*))x/ui, "\u{10401}\u{10400}\u{10406}x", ["x", "\u{10400}\u{10406}"]);
testRegExp(/(?<=([^\u{10000}\u{10429}]*))x/u, "\u{10401}A\u{10400}\u{10406}x", ["x", "\u{10401}A\u{10400}\u{10406}"]);
testRegExp(/(?<=([^\u{10000}\u{10429}]*))x/ui, "\u{10401}A\u{10400}\u{10406}x", ["x", "A\u{10400}\u{10406}"]);

// Test 76
testRegExp(/(?<=(A[^\u{10000}\u{10429}]*))x/u, "A\u{10400}\u{10406}x", ["x", "A\u{10400}\u{10406}"]);
testRegExp(/(?<=(A[^\u{10000}\u{10429}]*))x/ui, "A\u{10400}\u{10406}x", ["x", "A\u{10400}\u{10406}"]);
testRegExp(/(?<=([\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040a}]))x/u, "\u{10408}x", ["x", "\u{10408}"]);
testRegExp(/(?<=([\u{10428}\u{1042a}\u{1042c}\u{1042e}]))x/ui, "\u{10404}x", ["x", "\u{10404}"]);
testRegExp(/(?<=(A[\u{10401}\u{10402}\u{10403}\u{10404}\u{10405}\u{10406}\u{10407}\u{10408}\u{10409}\u{1040a}]*))x/u, "A\u{10408}\u{10406}x", ["x", "A\u{10408}\u{10406}"]);

 // Test 81
testRegExp(/(?<=(A[\u{10428}\u{1042a}\u{1042c}\u{1042e}]*))x/ui, "A\u{10404}\u{10406}x", ["x", "A\u{10404}\u{10406}"]);
testRegExp(/(?<=(^[^\u{10000}\u{10429}]*))x/u, "\u{10401}A\u{10400}\u{10406}x", ["x", "\u{10401}A\u{10400}\u{10406}"]);
testRegExp(/(?<=((?:A|\u{10400})*))x/u, "\u{10400}A\u{10400}x", ["x", "\u{10400}A\u{10400}"]);
testRegExp(/(?<=((?:A|\u{10400}|\u{10401}|\u{10406})*))x/u, "\u{10401}A\u{10400}\u{10406}x", ["x", "\u{10401}A\u{10400}\u{10406}"]);
testRegExp(/(?<=(^(?:A|\u{10400}|\u{10401}|\u{10406})*))x/u, "\u{10401}A\u{10400}\u{10406}x", ["x", "\u{10401}A\u{10400}\u{10406}"]);

// Test 86
testRegExp(/(?<=(\d{2})(\d{2}))X/, "34121234X", ["X", "12", "34"]);
testRegExp(/(?<=\2\1(\d{2})(\d{2}))X/, "34121234X", ["X", "12", "34"]);
testRegExpSyntaxError(".(?<=.)?", "", "SyntaxError: Invalid regular expression: invalid quantifier");
