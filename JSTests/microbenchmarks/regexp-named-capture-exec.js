function test() {
    var re = /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/;
    var str = "Event on 2024-03-15 at noon";
    var result = 0;
    for (var i = 0; i < 500000; ++i) {
        var m = re.exec(str);
        result += m.groups.year.length;
    }
    return result;
}
noInline(test);
test();
