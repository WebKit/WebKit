(function() {
    var prototype;
    for (var i = 0; i < 2e5; ++i)
        prototype = (function() {}).prototype;
    if (!prototype)
        throw new Error("Bad assertion!");
})();
