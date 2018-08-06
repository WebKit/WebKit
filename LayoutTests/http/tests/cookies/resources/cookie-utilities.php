<?php
function startsWith($string, $substring)
{
    return substr($string, 0, strlen($substring)) === $substring;
}

function hostnameIsEqualToString($hostname)
{
    return startsWith($_SERVER["HTTP_HOST"], $hostname);
}

function resetCookies()
{
    if (hostnameIsEqualToString("127.0.0.1")) {
        resetCookiesForCurrentOrigin();
        header("Location: http://localhost:8000" . $_SERVER["PHP_SELF"]);
    } elseif (hostnameIsEqualToString("localhost")) {
        resetCookiesForCurrentOrigin();
        header("Location: http://127.0.0.1:8000" . $_SERVER["PHP_SELF"] . "?runTest");
    }
}

function shouldResetCookies()
{
    return empty($_SERVER["QUERY_STRING"]);
}

function wkSetCookie($name, $value, $additionalProperties)
{
    $cookieValue = $name . "=" . $value;
    foreach ($additionalProperties as $name => $value) {
        $cookieValue .= "; " . $name;
        if (isset($value))
            $cookieValue .= "=" . $value;
    }
    header("Set-Cookie: " . $cookieValue, FALSE /* replace */);
}

function deleteCookie($name)
{
    setcookie($name, "deleted", time() - 86400, "/");
}

function _deleteCookieCallback($value, $name)
{
    deleteCookie($name);
}

function resetCookiesForCurrentOrigin()
{
    array_walk($_COOKIE, _deleteCookieCallback);
}
?>
