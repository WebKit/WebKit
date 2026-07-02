jsTestIsAsync = true;

if (!(window.testRunner && window.internals)) {
    testFailed("Test requires both testRunner and internals");
    finishJSTest();
}

var allFrames;
var frameDocumentIDs = [];
var checkCount = 0;

function createFrames(framesToCreate)
{
    if (typeof framesToCreate !== "number")
        throw TypeError("framesToCreate must be a number.");

    allFrames = new Array(framesToCreate);

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

function iframeSentMessage(message)
{
    let iframe = iframeForMessage(message);
    let frameDocumentID = internals.documentIdentifier(iframe.contentWindow.document);
    frameDocumentIDs.push(frameDocumentID);

    iframe.src = "about:blank";

    if (frameDocumentIDs.length == allFrames.length) {
        handle = setInterval(() => {
            gc();
            if (++checkCount > 10) {
                clearInterval(handle);
                testFailed("All iframe documents leaked.");
                finishJSTest();
                return;
            }
            for (const documentID of frameDocumentIDs) {
                if (!internals.isDocumentAlive(documentID)) {
                    clearInterval(handle);
                    testPassed("The iframe document didn't leak.");
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
