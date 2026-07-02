(function() {
    var strings = [
        "   hello   ",
        "   " + "a".repeat(100) + "   ",
        "hello",
        "          ",
        "   \u65e5\u672c\u8a9e   ",
    ];

    var sum = 0;
    for (var i = 0; i < 5e6; i++)
        sum += strings[i % 5].trim().length;

    if (sum < 0)
        throw new Error("bad result: " + sum);
})();
