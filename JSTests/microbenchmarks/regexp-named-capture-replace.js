function test() {
    var re = /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/g;
    var str = "Dates: 2024-03-15, 2025-12-31, 2026-01-01.";
    var result = 0;
    function replacer(match, y, m, d, offset, string, groups) {
        return groups.month + "/" + groups.day + "/" + groups.year;
    }
    noInline(replacer);
    for (var i = 0; i < 100000; ++i)
        result += str.replace(re, replacer).length;
    return result;
}
noInline(test);
test();
