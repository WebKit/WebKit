(function() {
    var count = 0;
    Array.prototype.__defineSetter__("0", function(value) { count += value; });
    
    for (var i = 0; i < 10000; ++i) {
        var array = /foo/.exec("foo");
        if (array[0] != "foo")
            throw "Error: bad result: " + array[0];
        delete array[0];
        array[0] = 42;
        if (count != (i + 1) * 42)
            throw "Error: bad count at i = " + i + ": " + count;
    }
})();
 
