//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    for (var i = 0; i < 1000000; ++i)
        /foo/.test("foo");
})();

