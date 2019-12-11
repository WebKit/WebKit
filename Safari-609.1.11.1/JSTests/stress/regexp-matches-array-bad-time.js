(function() {
    var count = 11;

    var array;
    for (var i = 0; i < 10000; ++i) {
        array = /foo/.exec("foo");
        if (array[0] != "foo")
            throw "Error: bad result: " + array[0];
    }

    delete array[0];

    Array.prototype.__defineSetter__("0", function(value) { count += value; });
    
    array[0] = 42;
    if (count != 53)
        throw "Error: bad count: " + count;
})();

