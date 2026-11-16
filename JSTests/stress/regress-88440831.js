// This test passes if it does not crash in Debug builds

(function() {
    try {
        eval('\'\\\n\n\'');
    } catch {}

    try {
        new Function("/* comment */\n(/*;");
    } catch {}
})();