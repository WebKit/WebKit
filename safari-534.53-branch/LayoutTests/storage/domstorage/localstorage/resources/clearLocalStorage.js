function clearLocalStorage()
{
    var keys = new Array();
    for (key in localStorage)
        keys.push(key);
                
    for (key in keys)
        localStorage.removeItem(keys[key]);
}

if (window.localStorage)
    clearLocalStorage();
