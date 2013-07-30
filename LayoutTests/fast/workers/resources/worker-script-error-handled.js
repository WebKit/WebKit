onerror = function(message, url, lineno, colno)
{
    postMessage("onerror invoked for a script that has script error '" + message + "' at line " + lineno + " and column " + colno);
    return true;
}

if (true) foo.bar = 0;
