(function() {
    var shortAscii = "   hello   ";
    var longAscii = "   " + "a".repeat(100) + "   ";
    var noTrim = "hello";
    var allSpaces = "          ";
    var unicode = "   \u65e5\u672c\u8a9e   ";

    for (var i = 0; i < 1e6; i++) {
        shortAscii.trimEnd();
        longAscii.trimEnd();
        noTrim.trimEnd();
        allSpaces.trimEnd();
        unicode.trimEnd();
    }
})();
