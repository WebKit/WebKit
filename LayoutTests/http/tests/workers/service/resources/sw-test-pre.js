// All SW tests are async and text-only
if (window.testRunner) {
	testRunner.dumpAsText();
	testRunner.waitUntilDone();
}

function finishSWTest()
{
	if (window.testRunner)
		testRunner.notifyDone();
}
