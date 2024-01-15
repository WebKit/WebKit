const httpOnlyCookieName = "http-only-cookie";
const serverSideCookieName = "server-side-cookie";
const clientSideCookieName = "client-side-cookie";

function setEnableFeature(enable, completionHandler) {
    if (typeof completionHandler !== "function")
        testFailed("setEnableFeature() requires a completion handler function.");
    if (enable) {
        internals.setTrackingPreventionEnabled(true);
        testRunner.setStatisticsIsRunningTest(true);
        completionHandler();
    } else {
        testRunner.statisticsResetToConsistentState(function() {
            testRunner.setStatisticsIsRunningTest(false);
            internals.setTrackingPreventionEnabled(false);
            completionHandler();
        });
    }
}

async function resetCookiesITP() {
    var testURL = "http://127.0.0.1:8000";
    console.assert(testURL == document.location.origin);

    function setUp() {
        return new Promise((resolve) => {
            if (window.testRunner) {
                testRunner.setAlwaysAcceptCookies(true);
            }
            resolve();
        });
    }

    function cleanUp() {
        return new Promise((resolve) => {
            if (window.testRunner)
                testRunner.setAlwaysAcceptCookies(false);
            resolve();
        });
    }

    let promise = setUp();
    promise = promise.then(() => {
        return new Promise((resolve, reject) => {
            window.addEventListener("message", (messageEvent) => resolve(messageEvent), {capture: true, once: true});
            const element = document.createElement("iframe");
            element.src = "http://127.0.0.1:8000/cookies/resources/delete-cookie.py";
            document.body.appendChild(element);
        });
    });
    return promise.then(cleanUp);
}

function sortStringArray(a, b) {
    a = a.toLowerCase();
    b = b.toLowerCase();

    return a > b ? 1 : b > a ? -1 : 0;
}

function addLinebreakToOutput() {
    let element = document.createElement("br");
    output.appendChild(element);
}

function addOutput(message) {
    let element = document.createElement("div");
    element.innerText = message;
    output.appendChild(element);
}

function checkCookies(isAfterDeletion) {
    let unsortedTestPassedMessages = [];
    let cookies = internals.getCookies();
    let potentialCookies = { "http-only-cookie": 1, "server-side-cookie": 1, "client-side-cookie": 1 };
    if (!cookies.length)
        testFailed((isAfterDeletion ? "After" : "Before") + " script-accessible deletion: No cookies found.");
    for (let cookie of cookies) {
        switch (cookie.name) {
            case httpOnlyCookieName:
                delete potentialCookies[httpOnlyCookieName];
                unsortedTestPassedMessages.push((isAfterDeletion ? "After" : "Before") + " deletion: " + (isAfterDeletion ? " " : "") + "HttpOnly cookie exists.");
                break;
            case serverSideCookieName:
                delete potentialCookies[serverSideCookieName];
                unsortedTestPassedMessages.push((isAfterDeletion ? "After" : "Before") + " deletion: Regular server-side cookie exists.");
                break;
            case clientSideCookieName:
                delete potentialCookies[clientSideCookieName];
                unsortedTestPassedMessages.push((isAfterDeletion ? "After" : "Before") + " deletion: Client-side cookie exists.");
                break;
        }
    }

    for (let missingCookie in potentialCookies) {
        switch (missingCookie) {
            case httpOnlyCookieName:
                unsortedTestPassedMessages.push((isAfterDeletion ? "After" : "Before") + " deletion: " + (isAfterDeletion ? " " : "") + "HttpOnly cookie does not exist.");
                break;
            case serverSideCookieName:
                unsortedTestPassedMessages.push((isAfterDeletion ? "After" : "Before") + " deletion: Regular server-side cookie does not exist.");
                break;
            case clientSideCookieName:
                unsortedTestPassedMessages.push((isAfterDeletion ? "After" : "Before") + " deletion: Client-side cookie does not exist.");
                break;
        }
    }

    let sortedTestPassedMessages = unsortedTestPassedMessages.sort(sortStringArray);
    for (let testPassedMessage of sortedTestPassedMessages) {
        addOutput(testPassedMessage);
    }
}

function addFrameEventListener() {
    window.addEventListener('message', e => {
        if (e.data === "getItemLocalStorage")
            return e.source.postMessage(localStorage.getItem(storageKey), "*");
        if (e.data === "getItemSessionStorage")
            return e.source.postMessage(sessionStorage.getItem(storageKey), "*");
        if (e.data == "createindexedDB")
            createIDBDataStore(dbName, objectStoreName, storageKey, storageValue, (message) => e.source.postMessage(message, "*"));
        if (e.data == "checkindexedDBDoesExists")
            checkIDBDataStoreExists((message) => e.source.postMessage(message, "*"));
        if (e.data === "getExpectedValue")
            return e.source.postMessage(storageValue, "*");
    });
}

function createIDBDataStore(dbName, objectStoreName, key, value, callback) {
    let request = indexedDB.open(dbName);
    //request.onerror = callback("createIDBDataStore: Couldn't create indexedDB.");
    request.onupgradeneeded = function(event) {
        let db = event.target.result;
        let objStore = db.createObjectStore(objectStoreName, {autoIncrement: true});
        objStore.add(value, key);
        callback("sucessfully created indexeddb");
    }
}

function checkIDBDataStoreExists(callback) {
    let request = indexedDB.open(dbName);
    request.onerror = function () {
        callback("Couldn't open indexedDB.", "onerror");
    };
    request.onupgradeneeded = function () {
        callback("IDB entry does not exist.", "onupgradeneeded");
    };
    request.onsuccess = function (e) {
        let db = e.target.result;
        try {
            let objStore = db.transaction(objectStoreName).objectStore(objectStoreName);
            let cursorRequest = objStore.openCursor();
            cursorRequest.onsuccess = (ev) => callback(ev.target.result.value, "onsuccess");
            cursorRequest.onerror = () => callback("Couldn't open cursor", "onupgradeneeded");
        } catch (ex) {
            callback(`Exception thrown: ${ex.message}`, "onsuccess");
        }
    };
}

function initStorage(key, value) {
    localStorage.setItem(key, value);
    sessionStorage.setItem(key, value);
}

function checkFrameStorage(isAfterDeletion, frame, label, callback) {
    let storageValue;
    let localStorageItem;
    let sessionStorageItem;
    let indexedDBResult;
    let currentRequest = "getExpectedValue";

    function finish() {
        addOutput((isAfterDeletion ? "After deletion: " : "Before deletion: ") + label + " LocalStorage entry " + (storageValue && localStorageItem === storageValue ? "does" : "does not") + " exist.");
        addOutput((isAfterDeletion ? "After deletion: " : "Before deletion: ") + label + " SessionStorage entry " + (storageValue && sessionStorageItem === storageValue ? "does" : "does not") + " exist.");
        addOutput((isAfterDeletion ? "After deletion: " : "Before deletion: ") + label + " indexedDB entry " + (storageValue && indexedDBResult === storageValue ? "does" : "does not") + " exist.");
        window.removeEventListener("message", receiveResponse);
        callback();
    }

    function receiveResponse(e) {
        switch (currentRequest) {
        case "getExpectedValue": {
            storageValue = e.data;
            currentRequest = isAfterDeletion ? "getItemLocalStorage" : "createindexedDB";
            frame.postMessage(currentRequest, "*");
            break;
        }
        case "createindexedDB": {
            indexedDBResult = e.data;
            currentRequest = "getItemLocalStorage";
            frame.postMessage(currentRequest, "*");
            break;
        }
        case "getItemLocalStorage": {
            localStorageItem = e.data;
            currentRequest = "getItemSessionStorage";
            frame.postMessage(currentRequest, "*");
            break;
        }
        case "getItemSessionStorage": {
            sessionStorageItem = e.data;
            currentRequest = "checkindexedDBDoesExists";
            frame.postMessage(currentRequest, "*");
            break;
        }
        case "checkindexedDBDoesExists": {
            indexedDBResult = e.data;
            finish();
            break;
        }
        };
    }
    window.addEventListener("message", receiveResponse);
    frame.postMessage(currentRequest, "*");
}

async function writeWebsiteDataAndContinue() {
    // Write cookies.
    await fetch("/cookies/resources/set-http-only-cookie.py?cookieName=" + httpOnlyCookieName, { credentials: "same-origin" });
    await fetch("/cookies/resources/setCookies.cgi", { headers: { "X-Set-Cookie": serverSideCookieName + "=1; path=/;" }, credentials: "same-origin" });
    document.cookie = clientSideCookieName + "=1";

    checkCookies(false);

    // Write LocalStorage
    localStorage.setItem(storageKey, storageValue);
    addOutput("Before deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");

    // Write IndexedDB.
    createIDBDataStore(dbName, objectStoreName, storageKey, storageValue, function () {
        checkIDBDataStoreExists(function(message, eventName) {
            addOutput(`Before deletion: (${eventName}) IDB entry does ${message === storageValue ? "" : "not"} exist.`);
            addOutput(`Before deletion: ${crossOrigin} ${testRunner.isStatisticsHasLocalStorage(crossOrigin) ? "has" : "does not have"} local storage`);
            checkFrameStorage(false, iframe.contentWindow, "iframe", () => checkFrameStorage(false, popup, "popup", () => {
                addLinebreakToOutput();
                processWebsiteDataAndContinue();
            }));
        });
    });
}

function processWebsiteDataAndContinue() {
    testRunner.installStatisticsDidScanDataRecordsCallback(checkWebsiteDataAndContinue);
    testRunner.statisticsProcessStatisticsAndDataRecords();
}

function checkWebsiteDataAndContinue() {
    checkCookies(true);
    addOutput("After deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
    checkIDBDataStoreExists((message, eventName) => {
        addOutput(`After deletion: (${eventName}) IDB entry does ${message === storageValue ? "" : "not"} exist.`);
        addOutput(`After deletion: ${crossOrigin} ${testRunner.isStatisticsHasLocalStorage(crossOrigin) ? "has" : "does not have"} local storage`);
        // Log but don't continue on non-onsuccess callbacks
        if (eventName === "onerror" || eventName === "onupgradeneeded")
            return;
        checkFrameStorage(true, iframe.contentWindow, "iframe", () => checkFrameStorage(true, popup, "popup", finishTest));
    });
}

async function finishTest() {
    internals.settings.setStorageBlockingPolicy('AllowAll');
    await resetCookiesITP();
    testRunner.setStatisticsFirstPartyWebsiteDataRemovalMode(false, function() {
        setEnableFeature(false, function() {
            testRunner.notifyDone();
        });
    });
}
