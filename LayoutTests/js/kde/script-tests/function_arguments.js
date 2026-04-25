// check value of arguments inside recursion

var expected = [null,99,1,2,3,3,2,1,99,null];
var expno = 0;

var x = 0;
shouldBe("mf.arguments", "expected[expno++]");
function mf(a,b) {
  shouldBe("mf.arguments[0]", "expected[expno++]");
  x++;
  if (x < 4)
    mf(x,1);
  shouldBe("mf.arguments[0]", "expected[expno++]");
  return b;
}
mf(99);
shouldBe("mf.arguments", "expected[expno++]");

// Check that sloppy mode ES5 function doesn't have own 'arguments' and 'caller' properties
function f(a,b,c) {
  shouldBeFalse("f.hasOwnProperty('arguments')");
  shouldBeFalse("f.hasOwnProperty('caller')");
}
f(1,2,3);

// Check that parameter variables are bound to the corresponding
// elements in the arguments array
var arg0 = null;
var arg1 = null;
var arg2 = null;
var newarg0 = null;
var newarg1 = null;
var newarg2 = null;
var newx = null;
var arglength = 0;

function dupargs(x,x,x)
{
  arg0 = arguments[0];
  arg1 = arguments[1];
  arg2 = arguments[2];
  arglength = arguments.length;
  x = 999;
  newarg0 = arguments[0];
  newarg1 = arguments[1];
  newarg2 = arguments[2];
  arguments[2] = 888;
  newx = x;
}

dupargs(1,2,3);

shouldBe("arg0","1");
shouldBe("arg1","2");
shouldBe("arg2","3");
shouldBe("arglength","3");
shouldBe("newarg0","1");
shouldBe("newarg1","2");
shouldBe("newarg2","999");
shouldBe("newx","888");
