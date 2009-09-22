// String object
shouldBe("'abc'.length", "3");
shouldBe("(new String('abcd')).length", "4");
shouldBe("String('abcde').length", "5");

// String.charAt()
shouldBe("'abc'.charAt(0)", "'a'");
shouldBe("'abc'.charAt(1)", "'b'");
shouldBe("'abc'.charAt(-1)", "''");
shouldBe("'abc'.charAt(99)", "''");
shouldBe("'abc'.charAt()", "'a'");

// String.prototype.indexOf()
shouldBe("'ab'.indexOf('a')", "0");
shouldBe("'ab'.indexOf('b')", "1");
shouldBe("'ab'.indexOf('x')", "-1");
shouldBe("'ab'.indexOf('')", "0");
shouldBe("''.indexOf('')", "0");
shouldBe("'ab'.indexOf('a', -1)", "0");
shouldBe("'ab'.indexOf('b', 1)", "1");
shouldBe("'ab'.indexOf('a', 1)", "-1");
shouldBe("'  '.indexOf('', 1)", "1");

// String.prototype.search()
shouldBe("String('abc').search(/b/)", "1");
shouldBe("String('xyz').search(/b/)", "-1");

// String.prototype.match()
shouldBe("String('abcb').match(/b/) + ''", "'b'");
shouldBe("typeof String('abc').match(/b/)", "'object'");
shouldBe("'xyz'.match(/b/)", "null");
shouldBe("'xyz'.match(/b/g)", "null");
shouldBe("String('aabab'.match(/ab/g))", "'ab,ab'");
shouldBe("String('aabab'.match(/(a)(b)/))","'ab,a,b'");
shouldBe("String('aabab'.match(/(a)(b)/g))","'ab,ab'");
shouldBe("String('abc'.match(/./g))","'a,b,c'");
shouldBe("String('abc'.match(/.*/g))","'abc,'");
// match() doesn't modify lastIndex (at least in moz)
shouldBe("var reg = /ab/g; 'aabab'.match(reg); reg.lastIndex", "0");
shouldBe("var reg = /ab/g; 'aabab'.match(reg).length", "2");
shouldBe("var reg = /ab/g; 'xxx'.match(reg); reg.lastIndex", "0");
shouldBe("var reg = /ab/g; 'xxx'.match(reg)", "null");
shouldBe( "myRe=/d(b+)d/g;  'cdbbdbsbz'.match(myRe)[0]", "'dbbd'" );

// String.prototype.replace()
shouldBe("'abcd'.replace(/b./, 'xy')", "'axyd'");
shouldBe("'abcd'.replace('bc', 'x')", "'axd'");
shouldBe("'abcd'.replace('x', 'y')", "'abcd'");
shouldBe("'abcd'.replace(/(ab)(cd)/,'$2$1')", "'cdab'");
shouldBe("'abcd'.replace(/(ab)(cd)/,'$2$1$')", "'cdab$'");
shouldBe("'BEGINabcEND'.replace(/abc/,'x$')", "'BEGINx$END'");

var f2c_str, f2c_p1, f2c_offset, f2c_s;
function f2c(x) {
    var s = String(x)
    var test = /(\d+(?:\.\d*)?)F\b/g
    return s.replace(test,
          function (str,p1,offset,s) {
             f2c_str=str; f2c_p1=p1; f2c_offset=offset; f2c_s=s;
             return ((p1-32) * 5/9) + "C";
          }
       )
 }

shouldBe("f2c('The value is 212F')", "'The value is 100C'");
shouldBe("f2c_str", "'212F'");
shouldBe("f2c_p1", "'212'");
shouldBe("f2c_offset", "13");
shouldBe("f2c_s", "'The value is 212F'");

// String.prototype.split()
shouldBe("'axb'.split('x').length", "2");
shouldBe("'axb'.split('x')[0]", "'a'");
shouldBe("'axb'.split('x')[1]", "'b'");
shouldBe("String('abc'.split(''))", "'a,b,c'");
shouldBe("String('abc'.split(new RegExp()))", "'a,b,c'");
shouldBe("''.split('').length", "0");
shouldBe("'axb'.split('x', 0).length", "0");
shouldBe("'axb'.split('x', 0)[0]", "undefined");
shouldBe("'axb'.split('x', 1).length", "1");
shouldBe("'axb'.split('x', 99).length", "2");
shouldBe("'axb'.split('y') + ''", "'axb'");
shouldBe("'axb'.split('y').length", "1");
shouldBe("''.split('x') + ''", "''");
shouldBe("'abc'.split() + ''", "'abc'");
shouldBe("'axxb'.split(/x/) + ''", "'a,,b'");
shouldBe("'axxb'.split(/x+/) + ''", "'a,b'");
shouldBe("'axxb'.split(/x*/) + ''", "'a,b'");  // NS 4.7 is wrong here
// moved to evil-n.js shouldBe("''.split(/.*/).length", "0");

// String.prototype.slice()
shouldBe("'abcdef'.slice(2, 5)", "'cde'");
shouldBe("'abcdefghijklmnopqrstuvwxyz1234567890'.slice(-32, -6)",
     "'efghijklmnopqrstuvwxyz1234'");

shouldBe("'abC1'.toUpperCase()", "'ABC1'");
shouldBe("'AbC2'.toLowerCase()", "'abc2'");

// String.prototype.localeCompare()
// ### not really testing the locale aspect
shouldBe("'a'.localeCompare('a')", "0");
shouldBe("'a'.localeCompare('aa') < 0", "true");
shouldBe("'a'.localeCompare('x') < 0", "true");
shouldBe("'x'.localeCompare('a') > 0", "true");
shouldBe("''.localeCompare('')", "0");
shouldBe("''.localeCompare()", "0");
shouldBe("''.localeCompare(undefined)", "-1");
shouldBe("''.localeCompare(null)", "-1");
shouldBe("'a'.localeCompare('')", "1");
shouldBe("'a'.localeCompare()", "0");

// warning: prototype modification below
shouldBe("'abc'[0]", "'a'");
shouldBeUndefined("'abc'[-1]");
shouldBeUndefined("'abc'[-4]");
shouldBeUndefined("'abc'[10]");
String.prototype[10] = "x";
shouldBe("'abc'[10]", "'x'");

var foo = "This is a test.";
var bar = foo.link( "javascript:foo( 'This ', 'is ', 'a test' )");
var html = "<a href=\"javascript:foo( 'This ', 'is ', 'a test' )\">This is a test.</a>"
shouldBe("bar", "html");


successfullyParsed = true
