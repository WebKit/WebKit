function test(date)
{
    return date.toDateString();
}
noInline(test);

var date = new Date(2026, 9, 28);
for (var i = 0; i < 1e6; ++i)
    test(date);
