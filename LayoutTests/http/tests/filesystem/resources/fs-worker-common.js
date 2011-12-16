function debug(message)
{
    postMessage("MESG:" + message);
}

function finishJSTest()
{
    postMessage("DONE:");
}

function description(message)
{
    postMessage('DESC:' + message);
}

function testPassed(msg) {
    postMessage("PASS:" + msg);
}

function testFailed(msg) {
    postMessage("FAIL:" + msg);
}

function shouldBe(_a, _b)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBe() expects string arguments");
  var _av = eval(_a);
  var _bv = eval(_b);
  if (_av === _bv)
    testPassed(_a + " is " + _b);
  else
    testFailed(_a + " should be " + _bv + " (of type " + typeof _bv + "). Was " + _av + " (of type " + typeof _av + ").");
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }

function removeAllInDirectorySync(directory) {
    if (!directory)
        return;
    var reader = directory.createReader();
    do {
        var entries = reader.readEntries();
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].isDirectory)
                entries[i].removeRecursively();
            else
                entries[i].remove();
        }
    } while (entries.length);
}

if (this.importScripts && !this.requestFileSystem && !this.webkitRequestFileSystem) {
    debug('This test requires FileSystem API.');
    finishJSTest();
}
