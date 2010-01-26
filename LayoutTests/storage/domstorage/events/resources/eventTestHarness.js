if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

iframe = document.createElement("IFRAME");
iframe.src = "about:blank";
document.body.appendChild(iframe);
iframe.contentWindow.document.body.innerText = "Nothing to see here.";

storageEventList = new Array();
iframe.contentWindow.onstorage = function (e) {
    window.parent.storageEventList.push(e);
}

function runAfterStorageEvents(callback) {
    var currentCount = storageEventList.length;
    function onTimeout() {
        if (currentCount != storageEventList.length)
            runAfterStorageEvents(callback);
        else
            callback();
    }
    setTimeout(onTimeout, 0);
}

function testStorages(testCallback)
{
    // When we're done testing LocalStorage, this is run.
    function allDone()
    {
        debug("");
        debug("");
        window.successfullyParsed = true;
        isSuccessfullyParsed();
        debug("");
        if (window.layoutTestController)
            layoutTestController.notifyDone()
    }

    // When we're done testing with SessionStorage, this is run.
    function runLocalStorage()
    {
        debug("");
        debug("");
        testCallback("localStorage", allDone);
    }

    // First run the test with SessionStorage.
    testCallback("sessionStorage", runLocalStorage);
}
