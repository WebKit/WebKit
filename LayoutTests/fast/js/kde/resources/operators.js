// operator !
shouldBeTrue("!undefined");
shouldBeTrue("!null");
shouldBeTrue("!!true");
shouldBeTrue("!false");
shouldBeTrue("!!1");
shouldBeTrue("!0");
shouldBeTrue("!!'a'");
shouldBeTrue("!''");

// unary plus
shouldBe("+9", "9");
shouldBe("var i = 10; +i", "10");

// negation
shouldBe("-11", "-11");
shouldBe("var i = 12; -i", "-12");

// increment
shouldBe("var i = 0; ++i;", "1");
shouldBe("var i = 0; ++i; i", "1");
shouldBe("var i = 0; i++;", "0");
shouldBe("var i = 0; i++; i", "1");
shouldBe("var i = true; i++", "1");
shouldBe("var i = true; i++; i", "2");

// decrement
shouldBe("var i = 0; --i;", "-1");
shouldBe("var i = 0; --i; i", "-1");
shouldBe("var i = 0; i--;", "0");
shouldBe("var i = 0; i--; i", "-1");
shouldBe("var i = true; i--", "1");
shouldBe("var i = true; i--; i", "0");

// bitwise operators
shouldBe("~0", "-1");
shouldBe("~1", "-2");
shouldBe("~NaN", "-1");
shouldBe("~Infinity", "-1");
shouldBe("~Math.pow(2, 33)", "-1"); // 32 bit overflow
shouldBe("~(Math.pow(2, 32) + Math.pow(2, 31) + 2)",
         "2147483645"); // a signedness issue
shouldBe("~null", "-1");
shouldBe("3 & 1", "1");
shouldBe("2 | true", "3");
shouldBe("'3' ^ 1", "2");
shouldBe("3^4&5", "7");
shouldBe("2|4^5", "3");

shouldBe("1 << 2", "4");
shouldBe("8 >> 1", "4");
shouldBe("1 >> 2", "0");
shouldBe("-8 >> 24", "-1");
shouldBe("8 >>> 2", "2");
shouldBe("-8 >>> 24", "255");
shouldBe("(-2200000000 >> 1) << 1", "2094967296");
shouldBe("Infinity >> 1", "0");
shouldBe("Infinity << 1", "0");
shouldBe("Infinity >>> 1", "0");
shouldBe("NaN >> 1", "0");
shouldBe("NaN << 1", "0");
shouldBe("NaN >>> 1", "0");
shouldBe("8.1 >> 1", "4");
shouldBe("8.1 << 1", "16");
shouldBe("8.1 >>> 1", "4");
shouldBe("8.9 >> 1", "4");
shouldBe("8.9 << 1", "16");
shouldBe("8.9 >>> 1", "4");
shouldBe("Math.pow(2, 32) >> 1", "0");
shouldBe("Math.pow(2, 32) << 1", "0");
shouldBe("Math.pow(2, 32) >>> 1", "0");

// Try shifting by variables, to test non-constant-folded cases.
var one = 1;
var two = 2;
var twentyFour = 24;

shouldBe("1 << two", "4");
shouldBe("8 >> one", "4");
shouldBe("1 >> two", "0");
shouldBe("-8 >> twentyFour", "-1");
shouldBe("8 >>> two", "2");
shouldBe("-8 >>> twentyFour", "255");
shouldBe("(-2200000000 >> one) << one", "2094967296");
shouldBe("Infinity >> one", "0");
shouldBe("Infinity << one", "0");
shouldBe("Infinity >>> one", "0");
shouldBe("NaN >> one", "0");
shouldBe("NaN << one", "0");
shouldBe("NaN >>> one", "0");
shouldBe("888.1 >> one", "444");
shouldBe("888.1 << one", "1776");
shouldBe("888.1 >>> one", "444");
shouldBe("888.9 >> one", "444");
shouldBe("888.9 << one", "1776");
shouldBe("888.9 >>> one", "444");
shouldBe("Math.pow(2, 32) >> one", "0");
shouldBe("Math.pow(2, 32) << one", "0");
shouldBe("Math.pow(2, 32) >>> one", "0");

// addition
shouldBe("1+2", "3");
shouldBe("'a'+'b'", "'ab'");
shouldBe("'a'+2", "'a2'");
shouldBe("'2'+'-1'", "'2-1'");
shouldBe("true+'a'", "'truea'");
shouldBe("'a' + null", "'anull'");
shouldBe("true+1", "2");
shouldBe("false+null", "0");

// substraction
shouldBe("1-3", "-2");
shouldBe("isNaN('a'-3)", "true");
shouldBe("'3'-'-1'", "4");
shouldBe("'4'-2", "2");
shouldBe("true-false", "1");
shouldBe("false-1", "-1");
shouldBe("null-true", "-1");

// multiplication
shouldBe("2 * 3", "6");
shouldBe("true * 3", "3");
shouldBe("2 * '3'", "6");

// division
shouldBe("6 / 4", "1.5");
//shouldBe("true / false", "Inf");
shouldBe("'6' / '2'", "3");
shouldBeTrue("isNaN('x' / 1)");
shouldBeTrue("isNaN(1 / NaN)");
shouldBeTrue("isNaN(Infinity / Infinity)");
shouldBe("Infinity / 0", "Infinity");
shouldBe("-Infinity / 0", "-Infinity");
shouldBe("Infinity / 1", "Infinity");
shouldBe("-Infinity / 1", "-Infinity");
shouldBeTrue("1 / Infinity == +0");
shouldBeTrue("1 / -Infinity == -0"); // how to check ?
shouldBeTrue("isNaN(0/0)");
shouldBeTrue("0 / 1 === 0");
shouldBeTrue("0 / -1 === -0"); // how to check ?
shouldBe("1 / 0", "Infinity");
shouldBe("-1 / 0", "-Infinity");

// modulo
shouldBe("6 % 4", "2");
shouldBe("'-6' % 4", "-2");

shouldBe("2==2", "true");
shouldBe("1==2", "false");

shouldBe("1<2", "true");
shouldBe("1<=2", "true");
shouldBe("2<1", "false");
shouldBe("2<=1", "false");

shouldBe("2>1", "true");
shouldBe("2>=1", "true");
shouldBe("1>=2", "false");
shouldBe("1>2", "false");

shouldBe("'abc' == 'abc'", "true");
shouldBe("'abc' != 'xyz'", "true");
shouldBeTrue("true == true");
shouldBeTrue("false == false");
shouldBeTrue("true != false");
shouldBeTrue("'a' != null");
shouldBeTrue("'a' != undefined");
shouldBeTrue("null == null");
shouldBeTrue("null == undefined");
shouldBeTrue("undefined == undefined");
shouldBeTrue("NaN != NaN");
shouldBeTrue("true != undefined");
shouldBeTrue("true != null");
shouldBeTrue("false != undefined");
shouldBeTrue("false != null");
shouldBeTrue("'0' == 0");
shouldBeTrue("1 == '1'");
shouldBeTrue("NaN != NaN");
shouldBeTrue("NaN != 0");
shouldBeTrue("NaN != undefined");
shouldBeTrue("true == 1");
shouldBeTrue("true != 2");
shouldBeTrue("1 == true");
shouldBeTrue("false == 0");
shouldBeTrue("0 == false");

shouldBe("'abc' < 'abx'", "true");
shouldBe("'abc' < 'abcd'", "true");
shouldBe("'abc' < 'abc'", "false");
shouldBe("'abcd' < 'abcd'", "false");
shouldBe("'abx' < 'abc'", "false");

shouldBe("'abc' <= 'abc'", "true");
shouldBe("'abc' <= 'abx'", "true");
shouldBe("'abx' <= 'abc'", "false");
shouldBe("'abcd' <= 'abc'", "false");
shouldBe("'abc' <= 'abcd'", "true");

shouldBe("'abc' > 'abx'", "false");
shouldBe("'abc' > 'abc'", "false");
shouldBe("'abcd' > 'abc'", "true");
shouldBe("'abx' > 'abc'", "true");
shouldBe("'abc' > 'abcd'", "false");

shouldBe("'abc' >= 'abc'", "true");
shouldBe("'abcd' >= 'abc'", "true");
shouldBe("'abx' >= 'abc'", "true");
shouldBe("'abc' >= 'abx'", "false");
shouldBe("'abc' >= 'abx'", "false");
shouldBe("'abc' >= 'abcd'", "false");

// mixed strings and numbers - results validated in NS+moz+IE5
shouldBeFalse("'abc' <= 0"); // #35246
shouldBeTrue("'' <= 0");
shouldBeTrue("' ' <= 0");
shouldBeTrue("null <= 0");
shouldBeFalse("0 <= 'abc'");
shouldBeTrue("0 <= ''");
shouldBeTrue("0 <= null");
shouldBeTrue("null <= null");
shouldBeTrue("6 < '52'");
shouldBeTrue("6 < '72'"); // #36087
shouldBe("NaN < 0", "false");
shouldBe("NaN <= 0", "false");
shouldBe("NaN > 0", "false");
shouldBe("NaN >= 0", "false");

// strict comparison ===
shouldBe("0 === false", "false");
//shouldBe("undefined === undefined", "true"); // aborts in IE5 (undefined is not defined ;)
shouldBe("null === null", "true");
shouldBe("NaN === NaN", "false");
shouldBe("0.0 === 0", "true");
shouldBe("'abc' === 'abc'", "true");
shouldBe("'a' === 'x'", "false");
shouldBe("1 === '1'", "false");
shouldBe("'1' === 1", "false");
shouldBe("true === true", "true");
shouldBe("false === false", "true");
shouldBe("true === false", "false");
shouldBe("Math === Math", "true");
shouldBe("Math === Boolean", "false");
shouldBe("Infinity === Infinity", "true");

// !==
shouldBe("0 !== 0", "false");
shouldBe("0 !== 1", "true");

shouldBe("typeof undefined", "'undefined'");
shouldBe("typeof null", "'object'");
shouldBe("typeof true", "'boolean'");
shouldBe("typeof false", "'boolean'");
shouldBe("typeof 1", "'number'");
shouldBe("typeof 'a'", "'string'");
shouldBe("typeof shouldBe", "'function'");
shouldBe("typeof Number.NaN", "'number'");

shouldBe("11 && 22", "22");
shouldBe("null && true", "null");
shouldBe("11 || 22", "11");
shouldBe("null || 'a'", "'a'");

shouldBeUndefined("void 1");

shouldBeTrue("1 in [1, 2]");
shouldBeFalse("3 in [1, 2]");
shouldBeTrue("'a' in { a:1, b:2 }");

// instanceof
// Those 2 lines don't parse in Netscape...
shouldBe("(new Boolean()) instanceof Boolean", "true");
shouldBe("(new Boolean()) instanceof Number", "false");

var successfullyParsed = true;
