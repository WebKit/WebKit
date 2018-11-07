//@ skip if $architecture == "x86"

function test(a, b, c, d, e)
{
    return a.concat(b, c, d, e);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(new String("Cocoa"), new String("Cappuccino"), new String("Matcha"), new String("Rize"), new String("Kilimanjaro"));
