onerror = function(message, url, lineno, colno, error)
{
    if (url != location.href)
        postMessage("FAIL: Bad location. Actual: " + url + " Expected: " + location.href);
    splitUrl = url.split('/');
    postMessage("PASS: onerror in worker context invoked for a script that has script error '" + message + "' at " + splitUrl[splitUrl.length - 1] + ":" + lineno + ":" + colno + " with error object " + error);
    return false;
}

foo.bar = 0;
