//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function slice(string, start, end)
{
    return string.slice(start, end);
}
noInline(slice);

for (var i = 0; i < 1e6; ++i)
    slice("Cocoa", 2, 4);
