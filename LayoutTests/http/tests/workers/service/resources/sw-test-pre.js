// All SW tests are async and text-only
if (window.testRunner) {
	testRunner.dumpAsText();
	testRunner.waitUntilDone();
}

function log(msg)
{
    let console = document.getElementById("console");
    if (!console) {
        console = document.createElement("div");
        console.id = "console";
        document.body.appendChild(console);
    }
    let span = document.createElement("span");
    span.innerHTML = msg + "<br>";
    console.appendChild(span);
}

function finishSWTest()
{
	if (window.testRunner)
		testRunner.notifyDone();
}
