//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function toStringLeft(num)
{
    return num + 'Cocoa';
}

function toStringRight(num)
{
    return 'Cocoa' + num;
}
(function () {
    // Hoisting.
    for (var i = 0; i < 1e6; ++i) {
        toStringLeft(i);
        toStringRight(i);
    }
}());
