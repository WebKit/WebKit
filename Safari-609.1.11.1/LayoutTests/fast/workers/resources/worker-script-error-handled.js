onerror = function(message, url, lineno, colno, error)
{
    splitUrl = url.split('/');
    postMessage("onerror invoked for a script that has script error '" + message + "' at " + splitUrl[splitUrl.length - 1] + ":" + lineno + ":" + colno + " with error object " + error);
    return true;
}

if (true) foo.bar = 0;
