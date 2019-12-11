//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(a, b, c, d, e)
{
    return a.concat(b).concat(c).concat(d).concat(e);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(new String("Cocoa"), new String("Cappuccino"), new String("Matcha"), new String("Rize"), new String("Kilimanjaro"));
