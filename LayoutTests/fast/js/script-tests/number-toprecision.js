description(
    'This test checks a few Number.toPrecision cases, including ' +
    '<a href="http://bugs.webkit.org/show_bug.cgi?id=15145">15145: (0.999).toPrecision(1) returns incorrect result</a>' +
    '.');

shouldBeEqualToString("(0.999).toPrecision(1)", "1");
shouldBeEqualToString("(0.999).toPrecision(2)", "1.0");
shouldBeEqualToString("(0.999).toPrecision(3)", "0.999");

var posInf = 1/0;
var negInf = -1/0;
var nan = 0/0;

shouldBeEqualToString("(0.0).toPrecision(4)", "0.000");
shouldBeEqualToString("(-0.0).toPrecision(4)", "0.000");
shouldBeEqualToString("(0.0).toPrecision()", "0");
shouldBeEqualToString("(-0.0).toPrecision()", "0");
shouldBeEqualToString("(1234.567).toPrecision()", "1234.567");
shouldThrow("(1234.567).toPrecision(0)");
shouldThrow("(1234.567).toPrecision(null)"); // just like 0
shouldThrow("(1234.567).toPrecision(false)"); // just like 0
shouldThrow("(1234.567).toPrecision('foo')"); // just like 0
shouldThrow("(1234.567).toPrecision(-1)");
shouldBeEqualToString("(1234.567).toPrecision(1)", "1e+3");
shouldBeEqualToString("(1234.567).toPrecision(true)", "1e+3"); // just like 1
shouldBeEqualToString("(1234.567).toPrecision('1')", "1e+3"); // just like 1
shouldBeEqualToString("(1234.567).toPrecision(2)", "1.2e+3");
shouldBeEqualToString("(1234.567).toPrecision(2.9)", "1.2e+3");
shouldBeEqualToString("(1234.567).toPrecision(5)", "1234.6");
shouldBeEqualToString("(1234.567).toPrecision(21)", "1234.56700000000000728");
// SpiderMonkey allows precision values 1 to 100, we only allow 1 to 21 currently
shouldBeEqualToString("(1234.567).toPrecision(22)", "1234.567000000000007276");
shouldBeEqualToString("(1234.567).toPrecision(100)", "1234.567000000000007275957614183425903320312500000000000000000000000000000000000000000000000000000000");
shouldThrow("(1234.567).toPrecision(101)");

shouldThrow("(1234.567).toPrecision(posInf)");
shouldThrow("(1234.567).toPrecision(negInf)");
shouldThrow("(1234.567).toPrecision(nan)"); // nan is treated like 0

shouldBeEqualToString("posInf.toPrecision()", "Infinity");
shouldBeEqualToString("negInf.toPrecision()", "-Infinity");
shouldBeEqualToString("nan.toPrecision()", "NaN");

var successfullyParsed = true;
