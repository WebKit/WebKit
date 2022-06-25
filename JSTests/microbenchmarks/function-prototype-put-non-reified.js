(function() {
    var fn;
    var obj = {};
    for (var i = 0; i < 4e5; ++i) {
        fn = function() {};
        fn.prototype = obj;
    }
    if (fn.prototype !== obj)
        throw new Error("Bad assertion!");
})();
