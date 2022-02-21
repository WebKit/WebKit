(function() {
    var A;
    for (var i = 0; i < 2e5; ++i)
        A = class {};
    if (typeof A !== "function")
        throw new Error("Bad assertion!");
})();
