function clearSessionStorage()
{
    var keys = new Array();
    for (key in sessionStorage)
        keys.push(key);
                
    for (key in keys)
        sessionStorage.removeItem(keys[key]);
}

if (window.sessionStorage)
    clearSessionStorage();
