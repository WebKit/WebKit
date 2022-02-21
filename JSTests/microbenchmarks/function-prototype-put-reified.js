(function() {
    var fn = function() {};
    var obj = {};
    for (var i = 0; i < 4e5; ++i)
        fn.prototype = obj;
    if (fn.prototype !== obj)
        throw new Error("Bad assertion!");
})();
