var ORIGINAL_HOST  = "example.test";
var TEST_ROOT = "not-example.test";
var TEST_HOST = "cookies." + TEST_ROOT;
var TEST_SUB  = "subdomain." + TEST_HOST;

var STRICT_DOM = "strict_from_dom";
var IMPLICIT_STRICT_DOM = "implicit_strict_from_dom";
var IMPLICIT_NONE_DOM = "implicit_none_from_dom";
var STRICT_BECAUSE_INVALID_SAMESITE_VALUE = "strict_because_invalid_SameSite_value";
var NONE_BECAUSE_INVALID_SAMESITE_VALUE = "none_because_invalid_SameSite_value";
var LAX_DOM = "lax_from_dom";
var NORMAL_DOM = "normal_from_dom";

// Clear the three well-known cookies.
function clearKnownCookies() {
    var cookies = [ STRICT_DOM, LAX_DOM, NORMAL_DOM, IMPLICIT_STRICT_DOM, STRICT_BECAUSE_INVALID_SAMESITE_VALUE ];
    cookies.forEach(c => { document.cookie = c + "=0; expires=Thu, 01 Jan 1970 00:00:01 GMT; path=/"; });
}

function normalizeCookie(cookie)
{
    return cookie.split(/;\s*/).sort().join("; ");
}

function with_iframe(url) {
    return new Promise(function(resolve) {
        var frame = document.createElement('iframe');
        frame.src = url;
        frame.onload = function() { setTimeout(() => resolve(frame), 0); };
        document.body.appendChild(frame);
    });
}

function loadPopupThenTriggerPost()
{
    let finish;
    let promise = new Promise(resolve => finish = resolve);

    clearKnownCookies();
    document.cookie = LAX_DOM + "=1; SameSite=Lax; Max-Age=100; path=/";
    document.cookie = NORMAL_DOM + "=1; Max-Age=100; path=/";
    document.cookie = STRICT_DOM + "=1; SameSite=Strict; Max-Age=100; path=/";

    const opener = window.open("http://127.0.0.1:8000/cookies/resources/post-cookies-to-opener.py")
    window.onmessage = e => {
        window.onmessage = e => {
            opener.close();
            finish(e.data);
        };

        const newDoc = opener.document;
        var form = newDoc.createElement('form');
        form.method = 'POST';
        form.action = 'http://127.0.0.1:8000/cookies/resources/post-cookies-to-opener.py';
        var input = newDoc.createElement('input');
        input.name = 'name';
        input.value = 'value';
        form.appendChild(input);
        newDoc.body.appendChild(form);
        form.submit();
    };
    return promise;
}

function openPopupAndTriggerPost(popupURL, callback)
{
    let finish;
    let promise = new Promise(resolve => finish = resolve);

    clearKnownCookies();
    document.cookie = LAX_DOM + "=1; SameSite=Lax; Max-Age=100; path=/";
    document.cookie = NORMAL_DOM + "=1; Max-Age=100; path=/";
    document.cookie = STRICT_DOM + "=1; SameSite=Strict; Max-Age=100; path=/";

    window.addEventListener("message", e => {
        opener.close();
        finish(e.data);
    });

    const opener = window.open(popupURL)
    const newDoc = opener.document;
    var form = newDoc.createElement('form');
    form.method = 'POST';
    form.action = 'http://127.0.0.1:8000/cookies/resources/post-cookies-to-opener.py';
    var input = newDoc.createElement('input');
    input.name = 'name';
    input.value = 'value';
    form.appendChild(input);
    newDoc.body.appendChild(form);
    form.submit();

    return promise;
}
