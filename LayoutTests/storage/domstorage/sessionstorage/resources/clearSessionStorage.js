function clearSessionStorage()
{
    var i = 0;
    for (;i < sessionStorage.length; ++i)
        sessionStorage.removeItem(sessionStorage.key(i));
}

if (window.sessionStorage)
    clearSessionStorage();
