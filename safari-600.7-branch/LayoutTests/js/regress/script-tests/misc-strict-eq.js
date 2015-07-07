(function() {
    var array = [true, false, null, void 0];
    for (var i = 0; i < 1000000; ++i) {
        for (var j = 0; j < array.length; ++j) {
            for (var k = j ; k < array.length; ++k) {
                var actual = array[j] === array[k];
                var expected = j == k;
                if (actual != expected)
                    throw "Error: bad result for j = " + j + ", k = " + k;
            }
        }
    }
})();
