if (window.layoutTestController)
    layoutTestController.dumpAsText();

function description(msg)
{
    document.getElementById("description").innerHTML = '<p>' + msg + '</p><p>On success, you will see a series of "<span class="pass">PASS</span>" messages, followed by "<span class="pass">TEST COMPLETE</span>".</p>';
}

function debug(msg)
{
    var span = document.createElement("span");
    span.innerHTML = msg + '<br>';
    document.getElementById("console").appendChild(span);
}

function testPassed(msg)
{
    debug('<span class="pass">PASS</span> ' + msg + '</span>');
}

function testFailed(msg)
{
    debug('<span class="fail">FAIL</span> ' + msg + '</span>');
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

function shouldBeNaN(_a)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  if (exception)
    testFailed(_a + " should be NaN. Threw exception " + exception + ".");
  else if (isNaN(_av))
    testPassed(_a + " is NaN.");
  else
    testFailed(_a + " should be NaN. Was " + _av + ".");
}


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
    testFailed(_a + " should be undefined. Threw exception " + exception);
  else if (typeof _av == "undefined")
    testPassed(_a + " is undefined.");
  else
    testFailed(_a + " should be undefined. Was " + _av);
}


function shouldThrow(_a, _e)
{
  var exception;
  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
     exception = e;
  }

  var _ev;
  if (_e)
      _ev =  eval(_e);

  if (exception) {
    if (typeof _e == "undefined" || exception == _ev)
      testPassed(_a + " threw exception " + exception + ".");
    else
      testFailed(_a + " should throw exception " + _e + ". Threw exception " + exception + ".");
  } else if (typeof _av == "undefined")
    testFailed(_a + " should throw exception " + _e + ". Was undefined.");
  else
    testFailed(_a + " should throw exception " + _e + ". Was " + _av + ".");
}
