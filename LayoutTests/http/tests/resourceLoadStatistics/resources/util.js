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
    request.onupgradeneeded = function(event) {
        let db = event.target.result;
        let objStore = db.createObjectStore(objectStoreName, {autoIncrement: true});
        objStore.add(value, key);
        callback("successfully created indexeddb");
    }
}

function initStorage(key, value) {
    localStorage.setItem(key, value);
    sessionStorage.setItem(key, value);
}

function checkFrameStorage(isAfterDeletion, frame, label, callback) {
    // addOutput(isAfterDeletion);
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
        if (e.source !== frame) return;
        
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

function checkIDBDataStoreExists(callback) {
    const intervalMs = 200;
    const maxIntervals = 20;

    let tries = 0;
    let inFlight = false;
    let completed = false;
    let timer;

    const done = (message, eventName) => {
        if (completed) return;
        completed = true;
        clearInterval(timer);
        try { callback(message, eventName); } catch (_) {}
    };

    const tick = () => {
        if (completed) return;
        if (inFlight) return;
        if (++tries > maxIntervals) return done("Timed out checking IDB.", "timeout");

        inFlight = true;

        let req;
        try {
            req = indexedDB.open(dbName);
        } catch (ex) {
            inFlight = false;
            return done(`Exception thrown: ${ex.message}`, "exception");
        }

        req.onerror = () => {
            inFlight = false;
            done("Couldn't open indexedDB.", "onerror");
        };

        // DB absent → upgrade fires first. Finish ONCE here.
        req.onupgradeneeded = (e) => {
            try { e.target.result.close(); } catch (_) {}
            inFlight = false;
            done("IDB entry does not exist.", "onupgradeneeded");
        };

        req.onsuccess = (e) => {
            // If we already finished in onupgradeneeded, ignore.
            if (completed) { try { e.target.result.close(); } catch (_) {} return; }

            const db = e.target.result;
            try {
                if (!db.objectStoreNames.contains(objectStoreName)) {
                    try { db.close(); } catch (_) {}
                    inFlight = false;
                    return done("IDB entry does not exist.", "onsuccess");
                }

                const tx = db.transaction(objectStoreName, "readonly");
                const store = tx.objectStore(objectStoreName);
                const getReq = store.get(storageKey);

                getReq.onsuccess = (ev) => {
                    const val = ev.target.result; // undefined if key missing
                    try { db.close(); } catch (_) {}
                    inFlight = false;
                    if (completed) return;
                    done(val !== undefined ? val : "IDB entry does not exist.", "onsuccess");
                };

                getReq.onerror = () => {
                    try { db.close(); } catch (_) {}
                    inFlight = false;
                    done("Couldn't read object store.", "onerror");
                };
            } catch (ex) {
                try { db.close(); } catch (_) {}
                inFlight = false;
                done(`Exception thrown: ${ex.message}`, "exception");
            }
        };
    };

    timer = setInterval(tick, intervalMs);
    tick(); // kick immediately
}


function checkLocalStorageExists(isAfterDeletion, callback) {
    let maxIntervals = 20;
    let intervalCounterLocalStorage = 0;
    let checkLocalStorageIntervalID;
    checkLocalStorageCallback = callback;
    if (!isAfterDeletion) {
        // check until there is localStorage
        checkLocalStorageIntervalID = setInterval(function () {
            if (++intervalCounterLocalStorage >= maxIntervals) {
                clearInterval(checkLocalStorageIntervalID);
                addOutput("Before deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
                checkLocalStorageCallback();
            } else if (testRunner.isStatisticsHasLocalStorage(originUnderTest)) {
                clearInterval(checkLocalStorageIntervalID);
                addOutput("Before deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
                checkLocalStorageCallback();
            }
        }, 100);
    } else {
        // Check until there is NO LocalStorage.
        checkLocalStorageIntervalID = setInterval(function () {
            if (++intervalCounterLocalStorage >= maxIntervals) {
                clearInterval(checkLocalStorageIntervalID);
                addOutput("After deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
                checkLocalStorageCallback();
            } else if (!testRunner.isStatisticsHasLocalStorage(originUnderTest)) {
                clearInterval(checkLocalStorageIntervalID);
                addOutput("After deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
                checkLocalStorageCallback();
            }
        }, 100);
    }
}

async function writeWebsiteDataAndContinue() {
    // Write cookies.
    await fetch("/cookies/resources/set-http-only-cookie.py?cookieName=" + httpOnlyCookieName, { credentials: "same-origin" });
    await fetch("/cookies/resources/setCookies.cgi", { headers: { "X-Set-Cookie": serverSideCookieName + "=1; path=/;" }, credentials: "same-origin" });
    document.cookie = clientSideCookieName + "=1";

    checkCookies(false);

    // Write LocalStorage
    localStorage.setItem(storageKey, storageValue);
    checkLocalStorageExists(false, function () {
        
        // write IndexedDB
        createIDBDataStore(dbName, objectStoreName, storageKey, storageValue, function () {
            checkIDBDataStoreExists(function(message, eventName) {
                addOutput(`Before deletion: (${eventName}) IDB entry does ${message === storageValue ? "" : "not"} exist.`);
                addOutput(`Before deletion: ${crossOrigin} ${testRunner.isStatisticsHasLocalStorage(crossOrigin) ? "has" : "does not have"} local storage`);
                checkFrameStorage(false, iframe.contentWindow, "iframe", () => checkFrameStorage(false, popup, "popup", () => {
                    addLinebreakToOutput();
                    processWebsiteDataAndContinue(); // Kicks off the website-data removal pass
                }));
            });
        });
    });
}

async function processWebsiteDataAndContinue() {
    await testRunner.statisticsProcessStatisticsAndDataRecords();
    checkWebsiteDataAndContinue();
}

async function checkWebsiteDataAndContinue() {
    checkCookies(true);
    checkLocalStorageExists(true, function () {
        checkIDBDataStoreExists((message, eventName) => {
            addOutput(`After deletion: (${eventName}) IDB entry does ${message === storageValue ? "" : "not"} exist.`);
            addOutput(`After deletion: ${crossOrigin} ${testRunner.isStatisticsHasLocalStorage(crossOrigin) ? "has" : "does not have"} local storage`);
            checkFrameStorage(true, iframe.contentWindow, "iframe", () => checkFrameStorage(true, popup, "popup", finishTest));
        });
    });
}

async function finishTest() {
    internals.settings.setStorageBlockingPolicy('AllowAll');
    await resetCookiesITP();
    testRunner.setStatisticsFirstPartyWebsiteDataRemovalMode(false, function () {
        setEnableFeature(false, function () {
            testRunner.notifyDone();
        });
    });
}