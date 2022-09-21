// include fast/js/resources/js-test-pre.js before this file.
jsTestIsAsync = true;
let startingDocumentCount;
let endingDocumentCount;
const testingCount = 20;

async function doLeakTest(src, tolerance) {
    const frame = document.createElement('frame');
    document.body.appendChild(frame);

    function loadSourceIntoIframe(src) {
        return new Promise((resolve) => {
            var originalSrc = frame.src;
            frame.onload = resolve;
            frame.src = src;
        })
    }

    if (!window.internals) {
        debug("This test only runs on DumpRenderTree, as it requires existence of window.internals and cross-domain resource access check disabled.");
        finishJSTest();
    }

    await loadSourceIntoIframe('about:blank');
    startingDocumentCount = internals.numberOfLiveDocuments();
    for (let i = 0; i < testingCount; ++i) {
        await loadSourceIntoIframe(src);
        await loadSourceIntoIframe('about:blank');
        gc();
    }
    endingDocumentCount = internals.numberOfLiveDocuments();
    shouldBeTrue('endingDocumentCount - startingDocumentCount < testingCount * 0.8');
    finishJSTest();
}

function htmlToUrl(html) {
    return 'data:text/html;charset=utf-8,' + html;
}

function grabScriptText(id) {
    return document.getElementById(id).innerText;
}
// include fast/js/resources/js-test-post.js after this file.