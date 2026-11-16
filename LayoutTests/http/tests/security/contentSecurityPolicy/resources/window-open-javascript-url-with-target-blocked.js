function done()
{
    if (window.testRunner)
        testRunner.notifyDone();
}

window.open("javascript:window.opener.document.writeln('FAIL')", "child");
window.setTimeout(done, 0);
