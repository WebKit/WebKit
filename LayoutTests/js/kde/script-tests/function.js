///////////////////////////////
// empty bodies

function empty1() { ; }
shouldBe("empty1()", "undefined");

function empty2() { }
shouldBe("empty2()", "undefined");

var g = 2;
var obj = new Object();
with (obj) {
  obj.func = function(a) { return g*l*a; };
}
obj.l = 3;

shouldBe("obj.func(11)", "66");

///////////////////////////////

function ctor(xx) {
  this.x = xx;
  this.dummy = new Array();  // once broke the returned completion
}

c = new ctor(11);
shouldBe("c.x", "11");

///////////////////////////////
// anonymous functions
///////////////////////////////

f = function (arg) {
  return arg;
};


shouldBe("f('bbbbb')", "'bbbbb'");


function f() {return 1;};

// just verify that this can be parsed
Math.round(function anon() { });

///////////////////////////////
// arguments object
///////////////////////////////

function foo() {
  var result = "|";
  for (i = 0; i < arguments.length; i++)
    result += arguments[i] + "|";
  return result;
}

shouldBe("foo()", "'|'");
shouldBe("foo('bar')", "'|bar|'");
shouldBe("foo('bar', 'x')", "'|bar|x|'");

function foo2(a) {
  var i = 0;
  for(a in arguments) // should be enumerable
    i++;
  return i;
}

shouldBe("foo2(7)", "1");

// I have my doubts about the standard conformance of this
function foo3(i, j) {
  switch(i) {
    case 0:
       return foo3.arguments.length;
    case 1:
       return foo3.arguments;
    case 2:
       return foo3.j;
  }
}

shouldBe("foo3(0, 99)", "2");
shouldBe("foo3(1, 99).length", "2");
//shouldBe("foo3(2, 99)", "99"); // IE doesn't support this either

///////////////////////////////
// nested function declarations
///////////////////////////////

function nest0(a, b)
{
  function nest1(c) { return c*c; }
  return nest1(a*b);
}
shouldBe("nest0(2,3)", "36");

///////////////////////////////
// Function object
///////////////////////////////

shouldBe("(new Function('return 11;'))()", "11");
shouldBe("(new Function('', 'return 22;'))()", "22");
shouldBe("(new Function('a', 'b', 'return a*b;'))(11, 3)", "33");
shouldBe("(new Function(' a ', ' b ', 'return a*b;'))(11, 4)", "44");
shouldBe("(new Function(' a,b ', ' c ', 'return a*b;'))(11, 5)", "55");
shouldBe("(new Function(' a,b ', ' c3 ', 'return a*b;'))(11, 6)", "66");


///////////////////////////////
// length property
function funcp3(a, b, c) { }
shouldBe("funcp3.length", "3");
shouldBe("(function(a, b, c, d) { }).length", "4");
shouldBe("(new Function('a', 'b', '')).length", "2");

// recursive call
function f4(i)
{
   return i == 1 ? 1 : i*f4(i-1);
}

shouldBe("f4(4)", "24");

function f5(a) {
  return f6();
}

function f6() { 
  return f5.arguments[0];
}

shouldBe("f5(11)", "11");
shouldBe("f5.arguments", "null");

///////////////////////////////
// calling conventions
///////////////////////////////

function manipulate(x) {
   x[0] = 99;           // should operate on reference
   x = "nothing";       // should detach
}
var arr = new Array(1, 2, 3);
manipulate(arr);
shouldBe("arr[0]", "99");
arr = [1, 2, 3];
manipulate(arr);
shouldBe("arr[0]", "99");

///////////////////////////////
// toString() on functions
///////////////////////////////

function orgFunc(s) { return 'Hello ' + s; }
eval("var orgFuncClone = " + orgFunc);
shouldBe("typeof orgFuncClone", "'function'");
shouldBe("orgFuncClone('world')", "'Hello world'");

function groupFunc(a, b) { return (a+b)*3; } // check for () being preserved
eval("var groupClone = " + groupFunc);
shouldBe("groupClone(1, 2)", "9");

var sinStr = 'function sin() {\n    [native code]\n}'
shouldBe("String(Math.sin)", "sinStr");

///////////////////////////////
// shadowed functions
///////////////////////////////

function shadow() { return 1; }
function shadow() { return 2; }
shouldBe("shadow()", "2");

///////////////////////////////
// nested functions

var nest_property = "global nest";

function nested_outer() {

  var nest_property = "inner nest";

  function nested_inner() {
    return nest_property;
  }

  return nested_inner;
}

function not_nested() {
  return nest_property;
}

nested_outer.nest_property = "outer nest";

var nested = nested_outer();
var nestedret = nested();
shouldBe("nestedret","'inner nest'");

var not_nestedret = not_nested();
shouldBe("not_nestedret","'global nest'");

///////////////////////////////
// other properties
///////////////////////////////
function sample() { }
shouldBeUndefined("sample.prototype.prototype");
shouldBeTrue("sample.prototype.constructor == sample");

var caught = false;
try {
  sample.apply(1, 1); // invalid argument
} catch(dummy) {
  caught = true;
}
shouldBeTrue("caught");


function functionWithVarOverride(a) {
  var a = 2;
  return a;
}

shouldBe("functionWithVarOverride(1)", "2");
