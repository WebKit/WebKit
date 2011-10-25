description(
"The test verifies that the timer in iframe continues firing after iframe is adopted into a new window and the original window was closed."
);

var window1, iframe, window2;

function finish()
{
    // Timer may call this multiple times before actual shutdown, generating arbitrary number of log lines.
    // Reset the callback to avoid this.
    iframe.contentWindow.finish = null;

    testPassed("Received the timer beat from the adopted iframe - exiting.")
    window2.close();
    finishJSTest();
}

function page1Unloaded()
{
    testPassed("Page 1 is closed.");
    // Give the iframe a function to call from the timer.
    iframe.contentWindow.finish = finish;
}

function transferIframe()
{
    testPassed("Loaded page 2.");
    window2.adoptIframe(iframe);
    testPassed("Iframe transferred.");
    iframe.contentWindow.counter++;
    shouldBe("iframe.contentWindow.counter", "2");
    shouldBe("window2.location.href", "iframe.contentWindow.parent.location.href");
    window1.onunload = page1Unloaded;
    window1.close();
}

function window1Loaded()
{
    iframe = window1.document.getElementById("iframe");
    testPassed("Loaded iframe in window 1.");
    iframe.contentWindow.counter++;
    shouldBe("iframe.contentWindow.counter", "1");
    window2 = window.open("iframe-reparenting-new-page-2.html", "_blank");
    window2.addEventListener("load", transferIframe, false);
}

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.setCanOpenWindows();
}
window1 = window.open("resources/iframe-reparenting-new-page-1.html", "_blank");
window1.addEventListener("load", window1Loaded, false);

var jsTestIsAsync = true;
