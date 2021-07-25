noInline(Reflect.ownKeys);

(function() {
    var keys;
    for (var i = 0; i < 1e5; ++i)
        keys = Reflect.ownKeys(function() {});
    if (!keys)
        throw new Error("Bad assertion!");
})();
