var foo = function(o) {
    return o.baa;
};

noInline(foo);

(function() {
    var letters = ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"];
    var properties = [];
    var o = {};
    for (var i = 0; i < letters.length; ++i) {
        for (var j = 0; j < letters.length; ++j) {
            for (var k = 0; k < letters.length; ++k) {
                var property = letters[i] + letters[j] + letters[k];
                o[property] = i;
            }
        }
    }

    var keys = Object.keys(o);
    keys.sort();
    for (var i = keys.length - 1; i >= keys.length - 8000; i--) {
        delete o[keys[i]];
    }

    var sum = 0;
    var iVal = letters.indexOf("b");
    var niters = 1000;
    for (var i = 0; i < niters; ++i) {
        sum += foo(o);
    }

    if (sum != iVal * niters)
        throw new Error("incorrect result: " + sum);

    fullGC();
})();
