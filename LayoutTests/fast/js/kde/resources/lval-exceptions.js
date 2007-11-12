// Tests for raising --- and non-raising exceptions on access to reference to undefined things...

// Locals should throw on access if undefined..
fnShouldThrow(function() { a = x; }, ReferenceError);

// Read-modify-write versions of assignment should throw as well
fnShouldThrow(function() { x += "foo"; }, ReferenceError);

// Other reference types should just return undefined...
a = new Object();
fnShouldNotThrow(function() { b = a.x; });
fnShouldNotThrow(function() { b = a['x']; });
fnShouldNotThrow(function() { a['x'] += 'baz'; });
shouldBe("a['x']", '"undefinedbaz"');
fnShouldNotThrow(function() { b = a.y; });
fnShouldNotThrow(function() { a.y += 'glarch'; });
shouldBe("a['y']", '"undefinedglarch"');


// Helpers!
function fnShouldThrow(f, exType)
{
  var exception;
  var _av;
  try {
     _av = f();
  } catch (e) {
     exception = e;
  }

  if (exception) {
    if (typeof exType == "undefined" || exception instanceof exType)
      testPassed(f + " threw exception " + exception + ".");
    else
      testFailed(f + " should throw exception " + exType + ". Threw exception " + exception + ".");
  } else if (typeof _av == "undefined")
    testFailed(f + " should throw exception " + exType + ". Was undefined.");
  else
    testFailed(f + " should throw exception " + exType + ". Was " + _av + ".");
}

function fnShouldNotThrow(f)
{
  try {
    f();
    testPassed(f + " did not throw an exception");
  } catch (e) {
    testFailed(f + " threw an exception " + e + " when no exception expected");
  }
}

var successfullyParsed = true;
