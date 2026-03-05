(function() {
    // Non-Latin1 whitespace characters (Zs category + BOM)
    // These test the optimized isWhiteSpace path that avoids ICU calls
    // Longer whitespace sequences to better measure isWhiteSpace performance
    var ideographicSpace = "\u3000".repeat(20) + "hello" + "\u3000".repeat(20);
    var enQuad = "\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A".repeat(5) + "hello" + "\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A".repeat(5);
    var thinSpaces = "\u2009\u200A".repeat(20) + "hello" + "\u2009\u200A".repeat(20);
    var narrowNoBreak = "\u202F\u205F".repeat(20) + "hello" + "\u202F\u205F".repeat(20);
    var ogham = "\u1680".repeat(20) + "hello" + "\u1680".repeat(20);
    var bom = "\uFEFF".repeat(20) + "hello" + "\uFEFF".repeat(20);
    var mixed = "\u3000\u2000\u202F\u1680\u2009\u205F\uFEFF".repeat(10) + "hello" + "\u3000\u2000\u202F\u1680\u2009\u205F\uFEFF".repeat(10);

    for (var i = 0; i < 1e5; i++) {
        ideographicSpace.trim();
        enQuad.trim();
        thinSpaces.trim();
        narrowNoBreak.trim();
        ogham.trim();
        bom.trim();
        mixed.trim();
    }
})();
