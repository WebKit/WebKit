if (self.testRunner)
    testRunner.waitUntilDone();

var g_childWindow;
var g_cachedCookies;
var g_baseURLWhenFetchingCookies = "";
var g_baseDocumentWhenFetchingDOMCookies;

function createCookie(name, value, additionalProperties)
{
    let cookie = `${name}=${value}`;
    for (let propertyName in additionalProperties) {
        cookie += `; ${propertyName}`;
        let propertyValue = additionalProperties[propertyName];
        if (propertyValue != undefined)
            cookie += "=" + propertyValue;
    }
    return cookie;
}

function setBaseDocumentWhenFetchingDOMCookies(aDocument)
{
    g_baseDocumentWhenFetchingDOMCookies = aDocument;
}

function setDOMCookie(name, value, additionalProperties={})
{
    g_baseDocumentWhenFetchingDOMCookies.cookie = createCookie(name, value, additionalProperties);
}

function getDOMCookies()
{
    if (!g_baseDocumentWhenFetchingDOMCookies)
        g_baseDocumentWhenFetchingDOMCookies = document;
    let cookies = g_baseDocumentWhenFetchingDOMCookies.cookie.split("; ");
    let result = {};
    for (let keyAndValuePair of cookies) {
        let [key, value] = keyAndValuePair.split("=");
        result[key] = value;
    }
   return result;
}

async function setCookie(name, value, additionalProperties={})
{
    invalidateCachedCookies();

    let cookie = createCookie(name, value, additionalProperties);
    let promise = new Promise((resolved, rejected) => {
        let xhr = new XMLHttpRequest;
        xhr.open("GET", "/cookies/resources/setCookies.cgi");
        xhr.setRequestHeader("SET-COOKIE", cookie);
        xhr.onload = () => resolved(xhr.responseText);
        xhr.onerror = rejected;
        xhr.send(null);
    });
    return promise;
}

function disableSetAlwaysAcceptCookies() {
    if (window.testRunner)
        testRunner.setAlwaysAcceptCookies(false);
}

async function resetCookies(urls)
{
    let testingURLs = [
        "http://127.0.0.1:8000",
        "http://localhost:8000",
    ];

    urls = urls || testingURLs;
    console.assert(urls.length);

    function setUp() {
        return new Promise((resolve) => {
            if (window.testRunner) {
                testRunner.setCanOpenWindows(true);
                testRunner.setAlwaysAcceptCookies(true);
                testRunner.setPopupBlockingEnabled(false);
            }
            resolve();
        });
    }

    function cleanUp() {
        return new Promise((resolve) => {
            disableSetAlwaysAcceptCookies();
            g_childWindow.close();
            g_childWindow = null;
            resolve();
        });
    }

    let promise = setUp();
    for (let url of urls) {
        promise = promise.then(() => {
            return new Promise((resolve, reject) => {
                // FIXME: For some reason we get a SecurityError when passing childWindow to resolve() in Safari Version 11.0.3 (13604.5.6)
                // and not in Chrome Canary 67.0.3390.0 (why?). As a workaround, store the child window reference in a global variable.
                window.addEventListener("message", (messageEvent) => resolve(messageEvent), {capture: true, once: true});
                g_childWindow = window.open(url + "/cookies/resources/cookie-utility.php?queryfunction=deleteCookiesAndPostMessage", "reset");
                if (!g_childWindow)
                    reject(null);
            });
        });
    }
    return promise.then(cleanUp);
}

async function resetCookiesForCurrentOrigin()
{
    invalidateCachedCookies();

    let promise = new Promise((resolved, rejected) => {
        let xhr = new XMLHttpRequest;
        xhr.open("GET", "/cookies/resources/cookie-utility.php?queryfunction=deleteCookies");
        xhr.onload = (progressEvent) => {
            disableSetAlwaysAcceptCookies();
            resolved(progressEvent);
        };
        xhr.onerror = (progressEvent) => {
            disableSetAlwaysAcceptCookies();
            rejected(progressEvent);
        };

        if (window.testRunner)
            testRunner.setAlwaysAcceptCookies(true);

        xhr.send(null);
    });
    return promise;
}

function setBaseURLWhenFetchingCookies(baseURLWhenFetchingCookies)
{
    g_baseURLWhenFetchingCookies = baseURLWhenFetchingCookies;
}

function invalidateCachedCookies()
{
    g_cachedCookies = null;
}

function _setCachedCookiesJSON(cookies)
{
    g_cachedCookies = JSON.parse(cookies);
}

async function getCookies()
{
    if (g_cachedCookies)
        return g_cachedCookies;

    let promise = new Promise((resolved, rejected) => {
        let xhr = new XMLHttpRequest;
        xhr.open("GET", `${g_baseURLWhenFetchingCookies}/cookies/resources/echo-json.php`);
        xhr.onload = () => resolved(xhr.responseText ? JSON.parse(xhr.responseText) : {});
        xhr.onerror = () => rejected({});
        xhr.send(null);
    });
    g_cachedCookies = await promise;
    return g_cachedCookies;
}

async function shouldNotHaveCookie(name)
{
    let cookies = await getCookies();
    let value = cookies[name];
    if (value == undefined)
        testPassed(`Do not have cookie "${name}".`);
    else
        testFailed(`Should not have cookie "${name}". But do with value ${value}.`);
}

async function shouldHaveCookie(name)
{
    let cookies = await getCookies();
    let value = cookies[name];
    if (value == undefined)
        testFailed(`Should have cookie "${name}". But do not.`);
    else
        testPassed(`Has cookie "${name}".`);
}

async function shouldHaveCookieWithValue(name, expectedValue)
{
    console.assert(expectedValue !== undefined);
    let cookies = await getCookies();
    let value = cookies[name];
    if (value == undefined)
        testFailed(`Should have cookie "${name}". But do not.`);
    else if (value === expectedValue)
        testPassed(`Has cookie "${name}" with value ${value}.`);
    else
        testFailed(`Cookie "${name}" should have value ${expectedValue}. Was ${value}.`);
}

function shouldNotHaveDOMCookie(name)
{
    let cookies = getDOMCookies();
    let value = cookies[name];
    if (value == undefined)
        testPassed(`Do not have DOM cookie "${name}".`);
    else
        testFailed(`Should not have DOM cookie "${name}". But do with value ${value}.`);
}

function shouldHaveDOMCookie(name)
{
    let cookies = getDOMCookies();
    let value = cookies[name];
    if (value == undefined)
        testFailed(`Should have DOM cookie "${name}". But do not.`);
    else
        testPassed(`Has DOM cookie "${name}".`);
}

function shouldHaveDOMCookieWithValue(name, expectedValue)
{
    console.assert(expectedValue !== undefined);
    let cookies = getDOMCookies();
    let value = cookies[name];
    if (value == undefined)
        testFailed(`Should have DOM cookie "${name}". But do not.`);
    else if (value === expectedValue)
        testPassed(`Has DOM cookie "${name}" with value ${value}.`);
    else
        testFailed(`DOM cookie "${name}" should have value ${expectedValue}. Was ${value}.`);
}
