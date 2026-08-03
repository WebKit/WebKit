function substring(string, start, end)
{
    return string.substring(start, end);
}
noInline(substring);

for (var i = 0; i < 1e6; ++i)
    substring("Cocoa", 3, 3);
