{
    function isArray(array)
    {
        return Array.isArray(array);
    }
    noInline(isArray);
    let proxy = new Proxy([], {});
    for (let i = 0; i < 1e5; ++i) {
        if (!isArray(proxy))
            throw new Error(`bad error`);
    }
}
{
    function isArray(array)
    {
        return Array.isArray(array);
    }
    noInline(isArray);
    let proxy = new Proxy({}, {});
    for (let i = 0; i < 1e5; ++i) {
        if (isArray(proxy))
            throw new Error(`bad error`);
    }
}
