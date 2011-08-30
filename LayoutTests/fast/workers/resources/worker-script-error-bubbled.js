onerror = function(message, url, lineno)
{
    if (url != location.href)
        postMessage("FAIL: Bad location. Actual: " + url + " Expected: " + location.href);
    splitUrl = url.split('/');
    postMessage("PASS: onerror in worker context invoked for a script that has script error '" + message + "' at line " + lineno + " in " + splitUrl[splitUrl.length - 1]);
    return false;
}

foo.bar = 0;
