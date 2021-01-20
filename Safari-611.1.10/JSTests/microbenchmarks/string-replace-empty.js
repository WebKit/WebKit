//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var result;
    for (var i = 0; i < 400000; ++i) {
        result = "foo".replace(/f/g, "");
    }
    if (result != "oo")
        throw "Error: bad result: "+ result;
})();
