var ORIGINAL_HOST  = "example.test";
var TEST_ROOT = "not-example.test";
var TEST_HOST = "cookies." + TEST_ROOT;
var TEST_SUB  = "subdomain." + TEST_HOST;

var STRICT_DOM = "strict_from_dom";
var IMPLICIT_STRICT_DOM = "implicit_strict_from_dom";
var STRICT_BECAUSE_INVALID_SAMESITE_VALUE = "strict_because_invalid_SameSite_value";
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

