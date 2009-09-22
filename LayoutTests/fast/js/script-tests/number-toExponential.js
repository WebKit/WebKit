var posInf = 1/0;
var negInf = -1/0;
var nan = 0/0;

// From Acid3, http://bugs.webkit.org/show_bug.cgi?id=16640
shouldBeEqualToString("(0.0).toExponential(4)", "0.0000e+0");
shouldBeEqualToString("(-0.0).toExponential(4)", "0.0000e+0");
shouldBeEqualToString("(0.0).toExponential()", "0e+0");
shouldBeEqualToString("(-0.0).toExponential()", "0e+0");
// From http://bugs.webkit.org/show_bug.cgi?id=5259
shouldBeEqualToString("(123.456).toExponential()", "1.23456e+2");
shouldBeEqualToString("(123.456).toExponential(0)", "1e+2");
// 0 equivilents
shouldBeEqualToString("(123.456).toExponential(null)", "1e+2");
shouldBeEqualToString("(123.456).toExponential(false)", "1e+2");
shouldBeEqualToString("(123.456).toExponential('foo')", "1e+2");
shouldBeEqualToString("(123.456).toExponential(nan)", "1e+2"); // nan is treated like 0

shouldBeEqualToString("(123.456).toExponential(1)", "1.2e+2");
// 1 equivilents
shouldBeEqualToString("(123.456).toExponential(true)", "1.2e+2"); // just like 1
shouldBeEqualToString("(123.456).toExponential('1')", "1.2e+2");

shouldBeEqualToString("(123.456).toExponential(2)", "1.23e+2");
shouldBeEqualToString("(123.456).toExponential(2.9)", "1.23e+2");
shouldBeEqualToString("(123.456).toExponential(3)", "1.235e+2");
shouldBeEqualToString("(123.456).toExponential(5)", "1.23456e+2");
shouldBeEqualToString("(123.456).toExponential(6)", "1.234560e+2");
shouldBeEqualToString("(123.456).toExponential(20)", "1.23456000000000003070e+2");

// SpiderMonkey allows precision values 0 to 100, we only allow 0 to 20 currently
shouldBeEqualToString("(123.456).toExponential(21)", "1.234560000000000030695e+2");
shouldBeEqualToString("(123.456).toExponential(100)", "1.2345600000000000306954461848363280296325683593750000000000000000000000000000000000000000000000000000e+2");
shouldThrow("(123.456).toExponential(101)");
shouldThrow("(123.456).toExponential(-1)");

shouldThrow("(1234.567).toExponential(posInf)");
shouldThrow("(1234.567).toExponential(negInf)");

shouldBeEqualToString("posInf.toExponential()", "Infinity");
shouldBeEqualToString("negInf.toExponential()", "-Infinity");
shouldBeEqualToString("nan.toExponential()", "NaN");

shouldBeEqualToString("(0.01).toExponential()", "1e-2");
shouldBeEqualToString("(0.1).toExponential()", "1e-1");
shouldBeEqualToString("(0.9).toExponential()", "9e-1");
shouldBeEqualToString("(0.9999).toExponential()", "9.999e-1");
shouldBeEqualToString("(0.9999).toExponential(2)", "1.00e+0");

var successfullyParsed = true;
