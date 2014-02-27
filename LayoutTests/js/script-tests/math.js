description(

"This test checks the behavior of the Math object as described in 15.8 of the language specification."

);

shouldBe("Math.abs(NaN)", "NaN");
shouldBe("Math.abs(0)", "0");
shouldBe("Math.abs(-0)", "0");
shouldBe("Math.abs(1)", "1");
shouldBe("Math.abs(-1)", "1");
shouldBe("Math.abs(Number.MIN_VALUE)", "Number.MIN_VALUE");
shouldBe("Math.abs(-Number.MIN_VALUE)", "Number.MIN_VALUE");
shouldBe("Math.abs(Number.MAX_VALUE)", "Number.MAX_VALUE");
shouldBe("Math.abs(-Number.MAX_VALUE)", "Number.MAX_VALUE");
shouldBe("Math.abs(Infinity)", "Infinity");
shouldBe("Math.abs(-Infinity)", "Infinity");

shouldBe("Math.acos(NaN)", "NaN");
shouldBe("Math.acos(-0)", "Math.acos(0)");
shouldBe("Math.acos(1)", "0");
shouldBe("Math.acos(1.1)", "NaN");
shouldBe("Math.acos(-1.1)", "NaN");
shouldBe("Math.acos(Infinity)", "NaN");
shouldBe("Math.acos(-Infinity)", "NaN");

shouldBe("Math.asin(NaN)", "NaN");
shouldBe("Math.asin(0)", "0");
shouldBe("Math.asin(-0)", "-0");
shouldBe("Math.asin(1)", "-Math.asin(-1)");
shouldBe("Math.asin(1.1)", "NaN");
shouldBe("Math.asin(-1.1)", "NaN");
shouldBe("Math.asin(Infinity)", "NaN");
shouldBe("Math.asin(-Infinity)", "NaN");

shouldBe("Math.atan(NaN)", "NaN");
shouldBe("Math.atan(0)", "0");
shouldBe("Math.atan(-0)", "-0");
shouldBe("Math.atan(Infinity)", "-Math.atan(-Infinity)");

shouldBe("Math.atan2(NaN, NaN)", "NaN");
shouldBe("Math.atan2(NaN, 0)", "NaN");
shouldBe("Math.atan2(NaN, -0)", "NaN");
shouldBe("Math.atan2(NaN, 1)", "NaN");
shouldBe("Math.atan2(NaN, Infinity)", "NaN");
shouldBe("Math.atan2(NaN, -Infinity)", "NaN");
shouldBe("Math.atan2(0, NaN)", "NaN");
shouldBe("Math.atan2(-0, NaN)", "NaN");
shouldBe("Math.atan2(1, NaN)", "NaN");
shouldBe("Math.atan2(Infinity, NaN)", "NaN");
shouldBe("Math.atan2(-Infinity, NaN)", "NaN");

// Regression test for Bug 26978 (https://bugs.webkit.org/show_bug.cgi?id=26978)
var testStr = "";
var v = { valueOf: function() { testStr += "one"; return 1; } };
var w = { valueOf: function() { testStr += "two"; return 2; } };
Math.atan2(v, w);
shouldBe('testStr', '\"onetwo\"');

/*
• Ify>0andxis+0, theresult isanimplementation-dependent approximationto +π/2. 
• Ify>0andxis−0, theresult isanimplementation-dependent approximationto +π/2. 
• Ifyis+0andx>0, theresult is+0. 
• Ifyis+0andxis+0, theresult is+0. 
• Ifyis+0andxis−0, theresult isanimplementation-dependent approximationto +π. 
• Ifyis+0andx<0, theresult isanimplementation-dependent approximationto +π. 
• Ifyis−0andx>0, theresult is−0. 
• Ifyis−0andxis+0, theresult is−0. 
• Ifyis−0andxis−0, theresult isanimplementation-dependent approximationto −π. 
• Ifyis−0andx<0, theresult isanimplementation-dependent approximationto −π. 
• Ify<0andxis+0, theresult isanimplementation-dependent approximationto −π/2. 
• Ify<0andxis−0, theresult isanimplementation-dependent approximationto −π/2. 
• Ify>0andyisfiniteandxis+∞, theresult is+0. 
• Ify>0andyisfiniteandxis−∞, theresult ifanimplementation-dependent approximationto +π. 
• Ify<0andyisfiniteandxis+∞, theresult is−0. 
• Ify<0andyisfiniteandxis−∞, theresult isanimplementation-dependent approximationto−π. 
• Ifyis+∞andxisfinite, theresult isanimplementation-dependent approximationto +π/2. 
• Ifyis−∞andxisfinite, theresult isanimplementation-dependent approximationto −π/2. 
• Ifyis+∞andxis+∞, theresult isanimplementation-dependent approximationto +π/4. 
• Ifyis+∞andxis−∞, theresult isanimplementation-dependent approximationto +3π/4. 
• Ifyis−∞andxis+∞, theresult isanimplementation-dependent approximationto −π/4. 
• Ifyis−∞andxis−∞, theresult isanimplementation-dependent approximationto −3π/4.
*/

shouldBe("Math.ceil(NaN)", "NaN");
shouldBe("Math.ceil(0)", "0");
shouldBe("Math.ceil(-0)", "-0");
shouldBe("Math.ceil(-0.5)", "-0");
shouldBe("Math.ceil(1)", "1");
shouldBe("Math.ceil(-1)", "-1");
shouldBe("Math.ceil(1.1)", "2");
shouldBe("Math.ceil(-1.1)", "-1");
shouldBe("Math.ceil(Number.MIN_VALUE)", "1");
shouldBe("Math.ceil(-Number.MIN_VALUE)", "-0");
shouldBe("Math.ceil(Number.MAX_VALUE)", "Number.MAX_VALUE");
shouldBe("Math.ceil(-Number.MAX_VALUE)", "-Number.MAX_VALUE");
shouldBe("Math.ceil(Infinity)", "Infinity");
shouldBe("Math.ceil(-Infinity)", "-Infinity");

shouldBe("Math.cos(NaN)", "NaN");
shouldBe("Math.cos(0)", "1");
shouldBe("Math.cos(-0)", "1");
shouldBe("Math.cos(Infinity)", "NaN");
shouldBe("Math.cos(-Infinity)", "NaN");

shouldBe("Math.exp(NaN)", "NaN");
shouldBe("Math.exp(0)", "1");
shouldBe("Math.exp(-0)", "1");
shouldBe("Math.exp(Infinity)", "Infinity");
shouldBe("Math.exp(-Infinity)", "0");

shouldBe("Math.floor(NaN)", "NaN");
shouldBe("Math.floor(0)", "0");
shouldBe("Math.floor(-0)", "-0");
shouldBe("Math.floor(0.5)", "0");
shouldBe("Math.floor(1)", "1");
shouldBe("Math.floor(-1)", "-1");
shouldBe("Math.floor(1.1)", "1");
shouldBe("Math.floor(-1.1)", "-2");
shouldBe("Math.floor(Number.MIN_VALUE)", "0");
shouldBe("Math.floor(-Number.MIN_VALUE)", "-1");
shouldBe("Math.floor(Number.MAX_VALUE)", "Number.MAX_VALUE");
shouldBe("Math.floor(-Number.MAX_VALUE)", "-Number.MAX_VALUE");
shouldBe("Math.floor(Infinity)", "Infinity");
shouldBe("Math.floor(-Infinity)", "-Infinity");

shouldBe("Math.log(NaN)", "NaN");
shouldBe("Math.log(0)", "-Infinity");
shouldBe("Math.log(-0)", "-Infinity");
shouldBe("Math.log(1)", "0");
shouldBe("Math.log(-1)", "NaN");
shouldBe("Math.log(-1.1)", "NaN");
shouldBe("Math.log(Infinity)", "Infinity");
shouldBe("Math.log(-Infinity)", "NaN");

shouldBe("Math.max()", "-Infinity");
shouldBe("Math.max(NaN)", "NaN");
shouldBe("Math.max(NaN,1)", "NaN");
shouldBe("Math.max(0)", "0");
shouldBe("Math.max(-0)", "-0");
shouldBe("Math.max(-0, 0)", "0");
shouldThrow("Math.max(NaN, {valueOf:function(){throw \"err\"}})","'err'");
shouldThrow("Math.max(NaN, NaN, {valueOf:function(){throw \"err\"}})","'err'");
shouldBe("Math.max(-0, NaN, 0)", "NaN");
shouldBe("Math.max(-0, NaN, 0, NaN)", "NaN");
sideEffect = 0;
shouldThrow("Math.max({valueOf:function(){throw \"error1\"}}, {valueOf:function(){sideEffect = 1}})", "'error1'")
shouldBe('sideEffect', '0');

shouldBe("Math.min()", "Infinity");
shouldBe("Math.min(NaN)", "NaN");
shouldBe("Math.min(NaN,1)", "NaN");
shouldBe("Math.min(0)", "0");
shouldBe("Math.min(-0)", "-0");
shouldBe("Math.min(-0, 0)", "-0");
shouldThrow("Math.min(NaN, {valueOf:function(){throw \"err\"}})","'err'");
shouldThrow("Math.min(NaN, NaN, {valueOf:function(){throw \"err\"}})","'err'");
shouldBe("Math.min(-0, NaN, 0)", "NaN");
shouldBe("Math.min(-0, NaN, 0, NaN)", "NaN");
sideEffect = 0;
shouldThrow("Math.min({valueOf:function(){throw \"error1\"}}, {valueOf:function(){sideEffect = 1}})", "'error1'")
shouldBe('sideEffect', '0');

shouldBe("Math.pow(NaN, NaN)", "NaN");
shouldBe("Math.pow(NaN, 0)", "1");
shouldBe("Math.pow(NaN, -0)", "1");
shouldBe("Math.pow(NaN, 1)", "NaN");
shouldBe("Math.pow(NaN, Infinity)", "NaN");
shouldBe("Math.pow(NaN, -Infinity)", "NaN");
shouldBe("Math.pow(0, NaN)", "NaN");
shouldBe("Math.pow(-0, NaN)", "NaN");
shouldBe("Math.pow(1, NaN)", "NaN");
shouldBe("Math.pow(Infinity, NaN)", "NaN");
shouldBe("Math.pow(-Infinity, NaN)", "NaN");

/*
• Ifabs(x)>1andyis+∞, theresult is+∞. 
• Ifabs(x)>1andyis−∞, theresult is+0. 
• Ifabs(x)==1andyis+∞, theresult isNaN. 
• Ifabs(x)==1andyis−∞, theresult isNaN. 
• Ifabs(x)<1andyis+∞, theresult is+0. 
• Ifabs(x)<1andyis−∞, theresult is+∞. 
• Ifxis+∞andy>0, theresult is+∞. 
• Ifxis+∞andy<0, theresult is+0. 
• Ifxis−∞andy>0andyisanoddinteger, theresult is−∞. 
• Ifxis−∞andy>0andyisnot anoddinteger, theresult is+∞. 
• Ifxis−∞andy<0andyisanoddinteger, theresult is−0. 
• Ifxis−∞andy<0andyisnot anoddinteger, theresult is+0. 
• Ifxis+0andy>0, theresult is+0. 
• Ifxis+0andy<0, theresult is+∞. 
• Ifxis−0andy>0andyisanoddinteger, theresult is−0. 
• Ifxis−0andy>0andyisnot anoddinteger, theresult is+0. 
• Ifxis−0andy<0andyisanoddinteger, theresult is−∞. 
• Ifxis−0andy<0andyisnot anoddinteger, theresult is+∞. 
• Ifx<0andxisfiniteandyisfiniteandyisnot aninteger, theresult isNaN.
*/

shouldBe("Math.round(NaN)", "NaN");
shouldBe("Math.round(0)", "0");
shouldBe("Math.round(-0)", "-0");
shouldBe("Math.round(0.4)", "0");
shouldBe("Math.round(-0.4)", "-0");
shouldBe("Math.round(0.5)", "1");
shouldBe("Math.round(-0.5)", "-0");
shouldBe("Math.round(0.6)", "1");
shouldBe("Math.round(-0.6)", "-1");
shouldBe("Math.round(1)", "1");
shouldBe("Math.round(-1)", "-1");
shouldBe("Math.round(1.1)", "1");
shouldBe("Math.round(-1.1)", "-1");
shouldBe("Math.round(1.5)", "2");
shouldBe("Math.round(-1.5)", "-1");
shouldBe("Math.round(1.6)", "2");
shouldBe("Math.round(-1.6)", "-2");
shouldBe("Math.round(8640000000000000)", "8640000000000000");
// The following is expected. Double can't represent .5 in this case.
shouldBe("Math.round(8640000000000000.5)", "8640000000000000");
shouldBe("Math.round(8640000000000001)", "8640000000000001");
shouldBe("Math.round(8640000000000002)", "8640000000000002");
shouldBe("Math.round(9007199254740990)", "9007199254740990");
shouldBe("Math.round(9007199254740991)", "9007199254740991");
shouldBe("Math.round(1.7976931348623157e+308)", "1.7976931348623157e+308");
shouldBe("Math.round(-8640000000000000)", "-8640000000000000");
shouldBe("Math.round(-8640000000000001)", "-8640000000000001");
shouldBe("Math.round(-8640000000000002)", "-8640000000000002");
shouldBe("Math.round(-9007199254740990)", "-9007199254740990");
shouldBe("Math.round(-9007199254740991)", "-9007199254740991");
shouldBe("Math.round(-1.7976931348623157e+308)", "-1.7976931348623157e+308");
shouldBe("Math.round(Infinity)", "Infinity");
shouldBe("Math.round(-Infinity)", "-Infinity");

shouldBe("Math.sin(NaN)", "NaN");
shouldBe("Math.sin(0)", "0");
shouldBe("Math.sin(-0)", "-0");
shouldBe("Math.sin(Infinity)", "NaN");
shouldBe("Math.sin(-Infinity)", "NaN");

shouldBe("Math.sqrt(NaN)", "NaN");
shouldBe("Math.sqrt(0)", "0");
shouldBe("Math.sqrt(-0)", "-0");
shouldBe("Math.sqrt(1)", "1");
shouldBe("Math.sqrt(-1)", "NaN");
shouldBe("Math.sqrt(Infinity)", "Infinity");
shouldBe("Math.sqrt(-Infinity)", "NaN");

shouldBe("Math.tan(NaN)", "NaN");
shouldBe("Math.tan(0)", "0");
shouldBe("Math.tan(-0)", "-0");
shouldBe("Math.tan(Infinity)", "NaN");
shouldBe("Math.tan(-Infinity)", "NaN");


shouldBe("Math.acosh(NaN)", "NaN");
shouldBe("Math.acosh(0)", "NaN");
shouldBe("Math.acosh(0.5)", "NaN");
shouldBe("Math.acosh(-1)", "NaN");
shouldBe("Math.acosh(Infinity)", "Infinity");
shouldBe("Math.acosh(-Infinity)", "NaN");


shouldBe("Math.asinh(NaN)", "NaN");
shouldBe("Math.asinh(0)", "0");
shouldBe("Math.asinh(-0)", "-0");
shouldBe("Math.asinh(Infinity)", "Infinity");
shouldBe("Math.asinh(-Infinity)", "-Infinity");

shouldBe("Math.atanh(NaN)", "NaN");
shouldBe("Math.atanh(0)", "0");
shouldBe("Math.atanh(-0)", "-0");
shouldBe("Math.atanh(-1)", "-Infinity");
shouldBe("Math.atanh(1)", "Infinity");
shouldBe("Math.atanh(Infinity)", "NaN");
shouldBe("Math.atanh(-Infinity)", "NaN");

shouldBe("Math.cbrt(NaN)", "NaN");
shouldBe("Math.cbrt(0)", "0");
shouldBe("Math.cbrt(-0)", "-0");
shouldBe("Math.cbrt(8)", "2");
shouldBe("Math.cbrt(-8)", "-2");
shouldBe("Math.cbrt(Infinity)", "Infinity");
shouldBe("Math.cbrt(-Infinity)", "-Infinity");

shouldBe("Math.cosh(NaN)", "NaN");
shouldBe("Math.cosh(0)", "1");
shouldBe("Math.cosh(-0)", "1");
shouldBe("Math.cosh(Infinity)", "Infinity");
shouldBe("Math.cosh(-Infinity)", "Infinity");

shouldBe("Math.expm1(NaN)", "NaN");
shouldBe("Math.expm1(0)", "0");
shouldBe("Math.expm1(-0)", "-0");
shouldBe("Math.expm1(Infinity)", "Infinity");
shouldBe("Math.expm1(-Infinity)", "-1");

shouldBe("Math.fround(NaN)", "NaN");
shouldBe("Math.fround(0)", "0");
shouldBe("Math.fround(-0)", "-0");
shouldBe("Math.fround(Infinity)", "Infinity");
shouldBe("Math.fround(-Infinity)", "-Infinity");

shouldBe("Math.log1p(NaN)", "NaN");
shouldBe("Math.log1p(0)", "0");
shouldBe("Math.log1p(-0)", "-0");
shouldBe("Math.log1p(Infinity)", "Infinity");
shouldBe("Math.log1p(-1)", "-Infinity");
shouldBe("Math.log1p(-Infinity)", "NaN");

shouldBe("Math.log10(NaN)", "NaN");
shouldBe("Math.log10(0)", "-Infinity");
shouldBe("Math.log10(-0)", "-Infinity");
shouldBe("Math.log10(10)", "1");
shouldBe("Math.log10(-1)", "NaN");
shouldBe("Math.log10(-Infinity)", "NaN");
shouldBe("Math.log10(Infinity)", "Infinity");
shouldBe("Math.log10(1)", "0");

shouldBe("Math.log2(NaN)", "NaN");
shouldBe("Math.log2(0)", "-Infinity");
shouldBe("Math.log2(-0)", "-Infinity");
shouldBe("Math.log2(2)", "1");
shouldBe("Math.log2(-1)", "NaN");
shouldBe("Math.log2(-Infinity)", "NaN");
shouldBe("Math.log2(Infinity)", "Infinity");
shouldBe("Math.log2(1)", "0");

shouldBe("Math.asinh(NaN)", "NaN");
shouldBe("Math.asinh(0)", "0");
shouldBe("Math.asinh(-0)", "-0");
shouldBe("Math.asinh(Infinity)", "Infinity");
shouldBe("Math.asinh(-Infinity)", "-Infinity");

shouldBe("Math.sinh(NaN)", "NaN");
shouldBe("Math.sinh(0)", "0");
shouldBe("Math.sinh(-0)", "-0");
shouldBe("Math.sinh(Infinity)", "Infinity");
shouldBe("Math.sinh(-Infinity)", "-Infinity");

shouldBe("Math.tanh(NaN)", "NaN");
shouldBe("Math.tanh(0)", "0");
shouldBe("Math.tanh(-0)", "-0");
shouldBe("Math.tanh(Infinity)", "1");
shouldBe("Math.tanh(-Infinity)", "-1");

shouldBe("Math.trunc(NaN)", "NaN");
shouldBe("Math.trunc(0)", "0");
shouldBe("Math.trunc(-0)", "-0");
shouldBe("Math.trunc(Infinity)", "Infinity");
shouldBe("Math.trunc(-Infinity)", "-Infinity");
shouldBe("Math.trunc(-0.5)", "-0");
shouldBe("Math.trunc(0.5)", "0");
shouldBe("Math.trunc(0xFFFFFFFF * 2 + 0.5)", "0xFFFFFFFF * 2");


var __Math = Math;
shouldBeTrue("delete Math;");

function global() { return this; }
shouldBeFalse("'Math' in global()");

Math = __Math;
