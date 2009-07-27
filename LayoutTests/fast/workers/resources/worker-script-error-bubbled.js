onerror = function(message, url, lineno)
{
    postMessage("onerror invoked for a script that has script error '" + message + "' at line " + lineno);
    return true;
}

foo.bar = 0;
