indexedDBTest(prepareDatabase);

function done() { finishJSTest(); }

function unexpectedErrorCallback() { alert("unexpected error"); done(); }

function getRequestCallback(event) {
    alert(event.target.result);
    done();
}

function dbOpenedSecondTime(event) {
    var getRequest = event.target.result.transaction(["foo"], "readonly").objectStore("foo").get("key");
    getRequest.onsuccess = getRequestCallback;
    getRequest.onerror = unexpectedErrorCallback;
}

function getValueFromIDB() {
    var openRequest = window.indexedDB.open(dbname);
    openRequest.onsuccess = dbOpenedSecondTime;
    openRequest.onblocked = unexpectedErrorCallback;
}

function makeDetachedFrame() {
    var iframe = document.getElementById('testIframe');
    iframe.contentWindow.postMessage('value1inDB', '*');

	setTimeout(getValueFromIDB, 500);
}

function prepareDatabase(event) {
    var request = event.target.result.createObjectStore("foo").add("original value", "key");
    request.onsuccess = makeDetachedFrame;
    request.onerror = unexpectedErrorCallback;
}
