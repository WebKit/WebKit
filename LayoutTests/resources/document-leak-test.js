jsTestIsAsync = true;

if (!window.testRunner || !window.internals) {
    testFailed("Test requires both testRunner and internals");
    finishJSTest();
}

var allFrames;
var frameDocumentIDs = [];
var checkCount = 0;

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

function iframeSentMessage(message)
{
    let iframe = iframeForMessage(message);
    let frameDocumentID = internals.documentIdentifier(iframe.contentWindow.document);
    frameDocumentIDs.push(frameDocumentID);

    iframe.src = "about:blank";

    if (frameDocumentIDs.length == allFrames.length) {
        handle = setInterval(() => {
            gc();
            for (const documentID of frameDocumentIDs) {
                if (!internals.isDocumentAlive(documentID)) {
                    clearInterval(handle);
                    testPassed("The iframe document didn't leak.");
                    finishJSTest();
                    return;
                }
                if (++checkCount > allFrames.length * 20) {
                    clearInterval(handle);
                    testFailed("All iframe documents leaked.");
                    finishJSTest();
                    return;
                }
            }
        }, 10);
    }
}

function runDocumentLeakTest(options)
{
    createFrames(options.framesToCreate);
    window.addEventListener("message", message => iframeSentMessage(message));
    allFrames.forEach(iframe => iframe.src = options.frameURL);
}
