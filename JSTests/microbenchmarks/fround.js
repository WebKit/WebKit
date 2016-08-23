(function() {
    var x = Math.fround(42.5);
    for (var i = 0; i < 3000000; ++i)
        x = Math.fround(Math.fround(Math.fround(i + 0.5) * 2.1353562) + x);
    
    if (x != 9609154658304)
        throw "Error: bad result: " + x;
})();
