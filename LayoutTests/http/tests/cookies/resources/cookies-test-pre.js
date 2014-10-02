document.write("<script src='/resources/js-test-pre.js'></script>");

var cookies = new Array();

// This method sets the cookies using XMLHttpRequest.
// We do not set the cookie right away as it is forbidden by the XHR spec.
// FIXME: Add the possibility to set multiple cookies in a row.
function setCookies(cookie)
{
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "resources/setCookies.cgi", false);
        xhr.setRequestHeader("SET-COOKIE", cookie);
        xhr.send(null);
        if (xhr.status == 200) {
            // This is to clear them later.
            cookies.push(cookie);
            return true;
        } else
            return false;
    } catch (e) {
        return false;
    }
}

function registerCookieForCleanup(cookie)
{
    cookies.push(cookie);
}

// Normalize a cookie string
function normalizeCookie(cookie)
{
    // Split the cookie string, sort it and then put it back together.
    return cookie.split('; ').sort().join('; ');
}

// We get the cookies throught an XMLHttpRequest.
function testCookies(result)
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "resources/getCookies.cgi", false);
    xhr.send(null);
    var cookie = xhr.getResponseHeader("HTTP_COOKIE") == null ? '"null"' : xhr.getResponseHeader("HTTP_COOKIE");

    // Normalize the cookie strings.
    result = normalizeCookie(result);
    cookie = normalizeCookie(cookie);
    
    if (cookie === result)
        testPassed("cookie is '" + cookie + "'.");
    else
        testFailed("cookie was '" + cookie + "'. Expected '" + result + "'.");
}

function clearAllCookies()
{
    // FIXME: This function is very wrong. If there is a cookie with a path (or any other
    // parameter) it will just spin forever. It is not possible to delete all cookies from
    // JavaScript, we should add a TestRunner API and switch to it.

    var cookieString;
    while (cookieString = document.cookie) {
        var cookieName = cookieString.substr(0, cookieString.indexOf("=") || cookieString.length());
        cookies.push(cookieName);
        clearCookies();

        // In case clearCookies.cgi failed, for example,
        // the domain/path do not match exactly:
        document.cookie = cookieName + "=;Max-Age=-1";
    }
}

function clearCookies()
{
    if (!cookies.length)
        return;

    try {
        var xhr = new XMLHttpRequest();
        var cookie;
        // We need to clean one cookie at a time because to be cleared the
        // cookie must be exactly the same except for the "Max-Age"
        // and "Expires" fields.
        while (cookie = cookies.pop()) {
            xhr.open("GET", "resources/clearCookies.cgi", false);
            xhr.setRequestHeader("CLEAR-COOKIE", cookie);
            xhr.send(null);
        }
    } catch (e) {
        debug("Could not clear the cookies expect the following results to fail");
    }
}

// This method check one cookie at a time.
function cookiesShouldBe(cookiesToSet, result)
{
    if (!setCookies(cookiesToSet)) {
        testFailed("could not set cookie(s) " + cookiesToSet);
        return;
    }
    testCookies(result);
}
