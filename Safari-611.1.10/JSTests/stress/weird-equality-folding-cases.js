function test(actualFunction, expected) {
    var actual = actualFunction();
    if (actual != expected)
        throw new Error("bad in " + actualFunction + " result: " + actual);
}

noInline(test);

for (var i = 0; i < 10000; ++i) {
    test(function() { return "5" == 5; }, true);
    test(function() { return ({valueOf:function(){return 42;}}) == 42; }, true);
    test(function() { return ({valueOf:function(){return 42;}}) == ({valueOf:function(){return 42;}}) }, false);
}
