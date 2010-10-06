function debug(message)
{
    postMessage(message);
}

function finishJSTest()
{
    postMessage("DONE");
}

function description(message)
{
    postMessage('Description: ' + message);
}

function shouldBe(_a, _b)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBe() expects string arguments");
  var _av = eval(_a);
  var _bv = eval(_b);
  if (_av === _bv)
    debug("PASS: " + _a + " is " + _b);
  else
    debug("FAIL: " + _a + " should be " + _bv + " (of type " + typeof _bv + "). Was " + _av + " (of type " + typeof _av + ").");
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }

if (this.importScripts && !this.requestFileSystem) {
    debug('This test requires FileSystem API.');
    finishJSTest();
}

onmessage = function(event) {
    importScripts(event.data);
}
