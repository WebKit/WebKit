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

// Talks to a target window (either popup or iframe)
function checkFrameStorage(isAfterDeletion, frame, label, callback, routeOrOptions) {  
    var expectedOrigin = null;

    if (typeof routeOrOptions === "boolean") {
        viaChild = routeOrOptions;
    } else if (typeof routeOrOptions === "string") {
        viaChild = (routeOrOptions === "child");
    } else if (routeOrOptions && typeof routeOrOptions === "object") {
        viaChild = !!routeOrOptions.viaChild;
        if (routeOrOptions.targetOrigin)   targetOrigin   = routeOrOptions.targetOrigin;
        if (routeOrOptions.expectedOrigin) expectedOrigin = routeOrOptions.expectedOrigin;
    }
    // addOutput(viaChild);
    // addOutput(label);
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
        if (e.source !== frame) return; // ignore any message not comming from expected frame
        if (expectedOrigin && e.origin !== expectedOrigin) return;
        
        // let oldRequest = currentRequest;
        let requestPayload = label === "popup-iframe" ? ("child:" + currentRequest) : currentRequest
        switch (currentRequest) {
        case "getExpectedValue": {
            storageValue = e.data;
            // addOutput(`${e.origin}: storageValue: ${storageValue}`);
            currentRequest = isAfterDeletion ? "getItemLocalStorage" : "createindexedDB";
            requestPayload = label === "popup-iframe" ? ("child:" + currentRequest) : currentRequest
            frame.postMessage(requestPayload, "*");
            break;
        }
        case "createindexedDB": {
            indexedDBResult = e.data;
            // addOutput(`${e.origin}: indexedDBResult: ${indexedDBResult}`);
            currentRequest = "getItemLocalStorage";
            requestPayload = label === "popup-iframe" ? ("child:" + currentRequest) : currentRequest
            frame.postMessage(requestPayload, "*");
            break;
        }
        case "getItemLocalStorage": {
            localStorageItem = e.data;
            // addOutput(`${e.origin}: localStorageItem: ${localStorageItem}`);
            currentRequest = "getItemSessionStorage";
            requestPayload = label === "popup-iframe" ? ("child:" + currentRequest) : currentRequest
            frame.postMessage(requestPayload, "*");
            break;
        }
        case "getItemSessionStorage": {
            sessionStorageItem = e.data;
            // addOutput(`${e.origin}: sessionStorageItem: ${sessionStorageItem}`);
            currentRequest = "checkindexedDBDoesExists";
            requestPayload = label === "popup-iframe" ? ("child:" + currentRequest) : currentRequest
            frame.postMessage(requestPayload, "*");
            break;
        }
        case "checkindexedDBDoesExists": {
            indexedDBResult = e.data;
            finish();
            break;
        }
        };
    }
    window.addEventListener("message", receiveResponse); // triggered when a message comes from opener
    const initialPayload = label === "popup-iframe" ? ("child:" + currentRequest): currentRequest;
    frame.postMessage(initialPayload, "*");
    // frame.postMessage(currentRequest, "*");
}

// target's listener
function addFrameEventListener() {
    window.addEventListener('message', e => {
        if (e.data === "getItemLocalStorage") {
            // console.log(`URL : ${document.URL} getItemLocalStorage  ${storageKey} + ${localStorage.getItem(storageKey)}`);
            let storageValue = localStorage.getItem(storageKey);

            return e.source.postMessage(storageValue === null ? "" : storageValue, "*");
        }
        if (e.data === "getItemSessionStorage")
            return e.source.postMessage(sessionStorage.getItem(storageKey), "*");
        if (e.data == "createindexedDB")
            createIDBDataStore(dbName, objectStoreName, storageKey, storageValue, (message) => e.source.postMessage(message, "*"));
        if (e.data == "checkindexedDBDoesExists")
            checkIDBDataStoreExists((message) => e.source.postMessage(message, "*"));
        if (e.data === "getExpectedValue") {
            // console.log(`origin: ${e.origin} storageKey: ${storageKey}`);
            return e.source.postMessage(storageValue, "*"); // comes from the target (popup or popup-iframe)
        }
    });
}

function checkCookies(isAfterDeletion) {
    // addOutput("checking cookies...");
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

function checkPopupCookiesLikeCheckCookies(isAfterDeletion, popupWin, done) {
  var finished = false;
  function finish() {
    if (finished) return;
    finished = true;
    window.removeEventListener("message", onMsg);
    if (typeof done === "function") done();
  }

  function onMsg(e){
    if (e.source !== popupWin) return;
    if (e.origin !== crossOrigin) return;
    const data = e.data;
    if (!data || data.type !== "cookies:list") return;

    const cookies = Array.isArray(data.cookies) ? data.cookies : [];
    const potential = { "http-only-cookie":1, "server-side-cookie":1, "client-side-cookie":1 };
    const msgs = [];
    const pfx = isAfterDeletion ? "After deletion popup:" : "Before deletion popup:";

    if (!cookies.length)
      testFailed(`${pfx} No cookies found.`);

    for (const c of cookies) {
      if (c.name in potential) delete potential[c.name];
      if (c.name === httpOnlyCookieName)
        msgs.push(`${pfx} HttpOnly cookie exists.`);
      else if (c.name === serverSideCookieName)
        msgs.push(`${pfx} Regular server-side cookie exists.`);
      else if (c.name === clientSideCookieName)
        msgs.push(`${pfx} Client-side cookie exists.`);
    }
    for (const missing in potential) {
      if (missing === httpOnlyCookieName)
        msgs.push(`${pfx} HttpOnly cookie does not exist.`);
      else if (missing === serverSideCookieName)
        msgs.push(`${pfx} Regular server-side cookie does not exist.`);
      else if (missing === clientSideCookieName)
        msgs.push(`${pfx} Client-side cookie does not exist.`);
    }

    msgs.sort(sortStringArray).forEach(addOutput);
    finish();
  }

  window.addEventListener("message", onMsg);

  setTimeout(function () {
    if (!finished) {
      addOutput("WARN: popup cookie query timed out; continuing.");
      finish();
    }
  }, 3000);

  try { popupWin.postMessage({ type: "get:cookies:all" }, crossOrigin); } catch (_){ finish(); }
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
    let maxIntervals = 30;
    let intervalCounterLocalStorage = 0;
    let checkLocalStorageIntervalID;
    checkLocalStorageCallback = callback;
    if (!isAfterDeletion) {
        checkLocalStorageIntervalID = setInterval(function () {
            if (++intervalCounterLocalStorage >= maxIntervals) {
                clearInterval(checkLocalStorageIntervalID);
                addOutput("Before deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
                addOutput("Before deletion: SessionStorage entry " + (sessionStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
                checkLocalStorageCallback();
            } else if (testRunner.isStatisticsHasLocalStorage(originUnderTest)) {
                clearInterval(checkLocalStorageIntervalID);
                addOutput("Before deletion: LocalStorage entry " + (localStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
                addOutput("Before deletion: SessionStorage entry " + (sessionStorage.getItem(storageKey) === storageValue ? "does" : "does not") + " exist.");
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

function waitForLocalStorage(frame, expectedOrigin, key, value, done, tries) {
    // addOutput("inside waitForLocalStorage");
    tries = tries || 0;
    if (tries > 30) { 
        return done();} // give up after ~3s

    function once(e) {
        if (e.source !== frame) return;
        if (expectedOrigin && e.origin !== expectedOrigin) return;
        window.removeEventListener("message", once);
        if (e.data === value) return done();
        setTimeout(function () { waitForLocalStorage(frame, expectedOrigin, key, value, done, tries + 1); }, 100);
    }
    window.addEventListener("message", once);
    frame.postMessage("getItemLocalStorage", "*");
}

async function phaseA_writeWebsiteDataAndReturn(callback) {
     // Write cookies.
    await fetch("/cookies/resources/set-http-only-cookie.py?cookieName=" + httpOnlyCookieName, { credentials: "same-origin" });
    await fetch("/cookies/resources/setCookies.cgi", { headers: { "X-Set-Cookie": serverSideCookieName + "=1; path=/;" }, credentials: "same-origin" });
    document.cookie = clientSideCookieName + "=1";

    checkCookies(false);

    // Write LocalStorage
    localStorage.setItem(storageKey, storageValue);
    sessionStorage.setItem(storageKey, storageValue);
    checkLocalStorageExists(false, function () {
        // write IndexedDB
        createIDBDataStore(dbName, objectStoreName, storageKey, storageValue, function () {
            checkIDBDataStoreExists(function(message, eventName) {
                addOutput(`Before deletion: (${eventName}) IDB entry does ${message === storageValue ? "" : "not"} exist.`);
                // addOutput(`Before deletion: ${crossOrigin} ${testRunner.isStatisticsHasLocalStorage(crossOrigin) ? "has" : "does not have"} local storage`);
                waitForLocalStorage(iframeWin, crossOrigin, storageKey, storageValue, function () {
                    checkFrameStorage(false, iframeWin, "iframe", () => {
                        addLinebreakToOutput();
                        callback();
                    }, false);
                });
            });
        });
    });
}

async function processWebsiteDataAndContinue(popupWindow, callback) {
    await testRunner.statisticsProcessStatisticsAndDataRecords();
    
    checkWebsiteDataAndContinue(popupWindow, callback);
}

async function checkWebsiteDataAndContinue(popupWindow, callback) {
    setTimeout(function () {
        checkPopupCookiesLikeCheckCookies(true, popupWindow, function () {
            checkFrameStorage(true, popupWindow, "popup", function () {
                checkFrameStorage(true, popupWindow, "popup-iframe", function () {
                    if (typeof callback === "function") callback();
                }, { viaChild: true, targetOrigin: crossOrigin, expectedOrigin: crossOrigin });
            }, { targetOrigin: crossOrigin, expectedOrigin: crossOrigin });
        });
    }, 600);
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