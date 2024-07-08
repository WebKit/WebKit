jsTestIsAsync = true;

if (!(window.testRunner && window.internals)) {
    testFailed("Test requires both testRunner and internals");
    finishJSTest();
}

var allFrames;
var maxFailCount = 0;

function createFrames(framesToCreate)
{
    if (typeof framesToCreate !== "number")
        throw TypeError("framesToCreate must be a number.");

    allFrames = new Array(framesToCreate);
    maxFailCount = framesToCreate;

    for (let i = 0; i < allFrames.length; ++i) {
        let frame = document.createElement("iframe");
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

            if (++checkCount > 5) {
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
