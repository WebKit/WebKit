//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    class Foo { }
    var foo = new Foo();
    
    for (var i = 0; i < 5000000; ++i) {
        if (!(foo instanceof Foo))
            throw "Error: bad result";
    }
})();
