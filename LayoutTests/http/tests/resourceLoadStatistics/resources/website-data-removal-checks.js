// Helpers for asserting whether ITP removed script-written website data.
//
// Assertions use a source of truth outside this web process, because an in-process read can briefly
// lag behind removal and report data that is already gone.

const removalPollIntervalMs = 100;
const removalPollTimeoutMs = 4000;
const retentionSettleMs = 500;

function rdcAddOutput(message) {
    let element = document.createElement("div");
    element.innerText = message;
    document.getElementById("output").appendChild(element);
}

function rdcSleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function rdcPollUntil(predicate) {
    const start = Date.now();
    while (Date.now() - start < removalPollTimeoutMs) {
        if (await predicate())
            return { satisfied: true, elapsedMs: Date.now() - start };
        await rdcSleep(removalPollIntervalMs);
    }
    return { satisfied: false, elapsedMs: Date.now() - start };
}

// ---------------------------------------------------------------- LocalStorage

// Queries storageManager()->fetchData() in the network process.
function rdcLocalStorageNetworkView(origin) {
    return testRunner.isStatisticsHasLocalStorage(origin);
}

function rdcLocalStorageWebView(name, expectedValue) {
    return localStorage.getItem(name) === expectedValue;
}

async function rdcCheckLocalStorage(phase, origin, name, expectedValue, shouldBeRemoved) {
    if (shouldBeRemoved) {
        let result = await rdcPollUntil(() => !rdcLocalStorageNetworkView(origin));
        if (!result.satisfied) {
            rdcAddOutput(phase + ": FAIL LocalStorage was NOT removed - timed out after " + removalPollTimeoutMs
                + "ms (network process still reports it present; web process value present: "
                + rdcLocalStorageWebView(name, expectedValue) + ").");
            return;
        }
        // The cached StorageArea is invalidated shortly after the network process drops the data, so
        // let the two views converge before calling it a mismatch.
        let converged = await rdcPollUntil(() => !rdcLocalStorageWebView(name, expectedValue));
        if (!converged.satisfied) {
            rdcAddOutput(phase + ": FAIL LocalStorage MISMATCH - network process reported it removed "
                + "but this web process still returned the value " + removalPollTimeoutMs + "ms later.");
            return;
        }
        rdcAddOutput(phase + ": LocalStorage entry does not exist.");
        return;
    }

    await rdcSleep(retentionSettleMs);
    let network = rdcLocalStorageNetworkView(origin);
    let web = rdcLocalStorageWebView(name, expectedValue);
    if (network !== web) {
        rdcAddOutput(phase + ": FAIL LocalStorage MISMATCH - network process present: " + network
            + ", web process present: " + web + ".");
        return;
    }
    rdcAddOutput(phase + ": LocalStorage entry " + (network ? "does" : "does NOT (unexpected)") + " exist.");
}

// -------------------------------------------------------------------- Cookies

// The server reports the Cookie header it actually received. A cookie written by document.cookie
// defaults to the current directory path, so it needs an explicit "path=/" to be sent here.
async function rdcCookieNamesSeenByServer() {
    let response = await fetch("/cookies/resources/echo-cookies.py", { credentials: "same-origin" });
    let text = await response.text();
    let names = [];
    for (let match of text.matchAll(/([A-Za-z0-9_\-]+)\s*=/g))
        names.push(match[1]);
    return names;
}

function rdcCookieNamesSeenByWebProcess() {
    return internals.getCookies().map((cookie) => cookie.name);
}

async function rdcCheckCookie(phase, cookieName, shouldBeRemoved) {
    const present = async () => (await rdcCookieNamesSeenByServer()).includes(cookieName);

    if (shouldBeRemoved) {
        let result = await rdcPollUntil(async () => !(await present()));
        if (!result.satisfied) {
            rdcAddOutput(phase + ": FAIL cookie '" + cookieName + "' was NOT removed - timed out after "
                + removalPollTimeoutMs + "ms (still sent to the server).");
            return;
        }
        let converged = await rdcPollUntil(() => !rdcCookieNamesSeenByWebProcess().includes(cookieName));
        if (!converged.satisfied) {
            rdcAddOutput(phase + ": FAIL cookie '" + cookieName + "' MISMATCH - no longer sent to the "
                + "server but internals.getCookies() still listed it " + removalPollTimeoutMs + "ms later.");
            return;
        }
        rdcAddOutput(phase + ": cookie '" + cookieName + "' does not exist.");
        return;
    }

    await rdcSleep(retentionSettleMs);
    let server = await present();
    let web = rdcCookieNamesSeenByWebProcess().includes(cookieName);
    if (server !== web) {
        rdcAddOutput(phase + ": FAIL cookie '" + cookieName + "' MISMATCH - sent to server: " + server
            + ", listed by web process: " + web + ".");
        return;
    }
    rdcAddOutput(phase + ": cookie '" + cookieName + "' " + (server ? "does" : "does NOT (unexpected)") + " exist.");
}

// ------------------------------------------------------------------ IndexedDB

// Read-only. indexedDB.open() creates the database, so it cannot be used to test for absence.
async function rdcIDBExists(dbName) {
    let databases = await indexedDB.databases();
    return databases.some((database) => database.name === dbName);
}

async function rdcCheckIDB(phase, dbName, shouldBeRemoved) {
    if (shouldBeRemoved) {
        let result = await rdcPollUntil(async () => !(await rdcIDBExists(dbName)));
        if (!result.satisfied) {
            rdcAddOutput(phase + ": FAIL IDB '" + dbName + "' was NOT removed - timed out after "
                + removalPollTimeoutMs + "ms (still listed by indexedDB.databases()).");
            return;
        }
        rdcAddOutput(phase + ": IDB entry does not exist.");
        return;
    }

    await rdcSleep(retentionSettleMs);
    rdcAddOutput(phase + ": IDB entry " + (await rdcIDBExists(dbName) ? "does" : "does NOT (unexpected)") + " exist.");
}

// Closes the connection so the test is not holding one open while measuring removal.
async function rdcCreateIDBAndClose(dbName) {
    let database = await new Promise((resolve, reject) => {
        let request = indexedDB.open(dbName);
        request.onerror = () => reject(request.error);
        request.onupgradeneeded = (event) => {
            event.target.result.createObjectStore("test", { autoIncrement: true }).add("value");
        };
        request.onsuccess = (event) => resolve(event.target.result);
    });
    database.close();
}
