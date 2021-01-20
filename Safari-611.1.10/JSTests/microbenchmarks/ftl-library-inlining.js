//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    for (var i = 0 ; i < 10000000; i++)
        Math.random();
})();
