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

// Test 1
testRegExp(/c(?!(\D))|c/u, "abcdef", ["c", undefined]);
testRegExp(/c(?!(\D){3})|c/u, "abcdef", ["c", undefined]);
testRegExp(/c(?=(de)x)|c/u, "abcdef", ["c", undefined]);
testRegExp(/c(?=(def))x|c/u, "abcdef", ["c", undefined]);
testRegExp(/c(?=(def))x|c(?!(def))|c/, "abcdef", ["c", undefined, undefined]);

// Test 6
testRegExp(/(?<!(\D{3}))f|f/u, "abcdef", ["f", undefined]);
testRegExp(/(?<!(\D{3}))f/, "abcdef", null);
testRegExp(/(?<!(\D))f/u, "abcdef", null);
testRegExp(/(?<!(\D){3})f/u, "abcdef", null);
testRegExp(/(?<!(\D){3})f|f/u, "abcdef", ["f", undefined]);

// Test 11
testRegExp(/(?<=(\w){6})f/, "abcdef", null);
testRegExp(/f(?=(\w{6})})/, "abcdef", null);
testRegExp(/((?<!\D{3}))f|f/u, "abcdef", ["f", undefined]);
testRegExp(/(?<!(\D){3})f/, "abcdef", null);
testRegExp(/(?<!(\d){3})f/, "abcdef", ["f", undefined]);

// Test 16
testRegExp(/(?<!(\D){3})f/, "abcdef", null);
testRegExp(/(?<!(\D){3})f|f/, "abcdef", ["f", undefined]);
testRegExp(/((?<!\D{3}))f|f/, "abcdef", ["f", undefined]);
testRegExp(/(?<!(\w{3}))f(?=(\w{3}))|(?<=(\w+?))c(?=(\w{2}))|(?<=(\w{4}))c(?=(\w{3})$)/, "abcdef", ["c",undefined,undefined,"b","de",undefined,undefined]);
testRegExp(/abc|(efg).*\!|xyz/, "efg xyz", ["xyz", undefined]);
