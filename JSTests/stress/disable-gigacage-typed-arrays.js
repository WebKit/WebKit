//@ runNoisyTestWithEnv "disable-gigacage", "GIGACAGE_ENABLED=0"

(function() {
    function foo(array, i)
    {
        return array[i];
    }
    
    noInline(foo);
    
    var array = new Int32Array(1000);
    for (var i = 0; i < array.length; ++i)
        array[i] = 5 - i;
    for (var i = 0; i < 1000; ++i) {
        var result = 0;
        var expectedResult = 0;
        for (var j = 0; j < array.length; ++j) {
            result += foo(array, j);
            expectedResult += 5 - j;
        }
        if (result != expectedResult)
            throw new Error("Bad result: " + result);
    }
})();

