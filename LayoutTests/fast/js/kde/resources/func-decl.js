// We can't use normal shouldBe here, since they'd eval in the wrong context...
function shouldBeOfType(msg, val, type) {
  if (typeof(val) != type)
    testFailed(msg + ": value has type " + typeof(val) + " , not:" + type);
  else
    testPassed(msg);
}

function shouldBeVal(msg, val, expected) {
  if (val != expected)
    testFailed(msg + ": value is " + val + " , not:" + expected);
  else
    testPassed(msg);
}

f = "global";

function test() {
  try {
    shouldBeOfType("Function declaration takes effect at entry", f, "function");
  }
  catch (e) {
    testFailed("Scoping very broken!");
  }

  for (var i = 0; i < 3; ++i) {
    if (i == 0)
      shouldBeOfType("Decl not yet overwritten", f, 'function');
    else
      shouldBeOfType("Decl already overwritten", f, 'number');

    f = 3;
    shouldBeVal("After assign ("+i+")", f, 3);

    function f() {};
    shouldBeVal("function decls have no execution content", f, 3);

    f = 5;

    shouldBeVal("After assign #2 ("+i+")", f, 5);
  }
}

test();

var successfullyParsed = true;
