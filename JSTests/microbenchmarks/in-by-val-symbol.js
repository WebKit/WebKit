//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
const cocoaSymbol = Symbol('cocoa');
const cappuccinoSymbol = Symbol('cappuccino');

function test(object)
{
    if (cocoaSymbol in object)
        return object[cocoaSymbol];
    return 0;
}
noInline(test);

var o1 = {
    [cocoaSymbol]: 42
};
var o2 = {
    [cappuccinoSymbol]: 41
};

for (var i = 0; i < 1e6; ++i) {
    test(o1);
    test(o2);
}
