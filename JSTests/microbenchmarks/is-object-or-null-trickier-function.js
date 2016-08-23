var object = {};
var array = [];
var func = function() { };
var func2 = function() { return 32; };
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        var value1 = (i & 1) ? array : object;
        var value2 = (i & 1) ? func : func2;
        result += (typeof value1 == "object");
        result += (typeof value2 == "object") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
