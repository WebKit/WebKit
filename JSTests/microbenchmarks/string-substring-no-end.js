function substring(string, start)
{
    return string.substring(start);
}
noInline(substring);

for (var i = 0; i < 1e6; ++i)
    substring("Cocoa", 2);
