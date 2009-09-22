function shouldBe(a, b, c)
{
  if ( a == b )
   debug(c+" .......... PASS");
  else
   debug(c+" .......... FAIL");
}

function testThrow()
{
  var caught = false;
  try {
    throw 99;
  } catch (e) {
    caught = true;
  }
  shouldBe(caught, true, "testing throw()");
}

// same as above but lacking a semicolon after throw
function testThrow2()
{
  var caught = false;
  try {
    throw 99
  } catch (e) {
    caught = true;
  }
  shouldBe(caught, true, "testing throw()");
}

function testReferenceError()
{
  var err = "noerror";
  var caught = false;
  try {
    var dummy = nonexistant; // throws reference error
  } catch (e) {
    caught = true;
    err = e.name;
  }
  // test err
  shouldBe(caught, true, "ReferenceError");
}

function testFunctionErrorHelper()
{
   var a = b;  // throws reference error
}

function testFunctionError()
{
  var caught = false;
  try {
    testFunctionErrorHelper();
  } catch (e) {
    caught = true;
  }
  shouldBe(caught, true, "error propagation in functions");
}

function testMathFunctionError()
{
  var caught = false;
  try {
    Math();
  } catch (e) {
    debug("catch");
    caught = true;
  } finally {
    debug("finally");
  }
  shouldBe(caught, true, "Math() error");
}

function testWhileAbortion()
{
  var caught = 0;
  try { 
    while (a=b, 1) { 	// "endless error" in condition
      ;
    }
  } catch (e) {
    caught++;
  }

  try { 
    while (1) {
      var a = b;	// error in body
    }
  } catch (e) {
    caught++;
  }
  shouldBe(caught, 2, "Abort while() on error");
}

debug("Except a lot of errors. They should all be caught and lead to PASS");
testThrow();
testThrow2();
testReferenceError();
testFunctionError();
testMathFunctionError();
testWhileAbortion();

successfullyParsed = true
