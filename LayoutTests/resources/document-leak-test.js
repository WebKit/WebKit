jsTestIsAsync = true;

if (!window.testRunner || !window.internals) {
    testFailed("Test requires both testRunner and internals");
    finishJSTest();
}

var allFrames;
var maxFailCount = 0;

// Generally for these kinds of tests we want to create more than one frame.
// Since the GC is conservative and stack-scanning it could find on the stack something
// pointer-like to the document object we are testing and so through no fault of the code
// under test the document object won't be collected. By creating multiple iframes and
// testing those we can improve the robsutness of the test against flaky false-positive leaks.
// On my M1 Pro MBP I've seen the test pass with as few as 6 frames tested. Something like
// 10-20 frames under test seems to be a good amount to balance test robustness with test
// running time in the event of a test failure. Too many frames might mean the test doesn't
// complete before the test runner times out.
function createFrames(framesToCreate)
{
    if (typeof framesToCreate !== "number")
        throw TypeError("framesToCreate must be a number.");

    allFrames = new Array(framesToCreate);
    maxFailCount = framesToCreate;

    for (let i = 0; i < allFrames.length; ++i) {
        let frame = document.createElement("iframe");
        frame.style.width = '100vw';
        frame.style.height = '100vh';
        frame.style.border = 'none';
        document.body.appendChild(frame);
        allFrames[i] = frame;
    }
}

function iframeForMessage(message)
{
    return allFrames.find(frame => frame.contentWindow === message.source);
}

var failCount = 0;
function iframeLeaked()
{
    if (++failCount >= maxFailCount) {
        testFailed("All iframe documents leaked.");
        finishJSTest();
    }
}

function iframeSentMessage(message)
{
    if (message.data === "testFailed") {
        testFailed("Error loading the initial frameURL.");
        return finishJSTest();
    }

    let iframe = iframeForMessage(message);
    let frameDocumentID = internals.documentIdentifier(iframe.contentWindow.document);
    let checkCount = 0;

    iframe.addEventListener("load", () => {
        let handle = setInterval(() => {
            gc();
            if (!internals.isDocumentAlive(frameDocumentID)) {
                clearInterval(handle);
                testPassed("The iframe document didn't leak.");
                finishJSTest();
            }

            if (++checkCount > 50) {
                clearInterval(handle);
                iframeLeaked();
            }
        }, 10);
    }, { once: true });

    iframe.src = "about:blank";
}

function runDocumentLeakTest(options)
{
    createFrames(options.framesToCreate);
    window.addEventListener("message", message => iframeSentMessage(message));
    allFrames.forEach(iframe => iframe.src = options.frameURL);
}
