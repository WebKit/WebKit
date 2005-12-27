// Utility functions for JS test suite
// (c) 2001 Harri Porten <porten@kde.org>

function testPassed(msg)
{
  debug("PASS. " + msg);
}

function testFailed(msg)
{
  debug("FAIL. " + msg);
  // regtest.reportResult(false, msg);  
}

function shouldBe(_a, _b)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBe() expects string arguments");
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }
  var _bv = eval(_b);

  if (exception)
    testFailed(_a + " should be " + _bv + ". Threw exception " + exception);
  else if (_av === _bv)
    testPassed(_a + " is " + _b);
  else
    testFailed(_a + " should be " + _bv + ". Was " + _av);
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }

function shouldBeFalse(_a) { shouldBe(_a, "false"); }

function shouldBeUndefined(_a)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  if (exception)
    testFailed(_a + " should be " + _bv + ". Threw exception " + exception);
  else if (typeof _av == "undefined")
    testPassed(_a + " is undefined.");
  else
    testFailed(_a + " should be undefined. Was " + _av);
}
