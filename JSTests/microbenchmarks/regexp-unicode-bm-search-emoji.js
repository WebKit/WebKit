(function() {
    var post = "";
    var emojis = ["\u{1F600}", "\u{1F60D}", "\u{1F525}", "\u{1F389}", "\u{2728}"];
    for (var i = 0; i < 60; i++)
        post += emojis[i % emojis.length] + "\u3044\u3044\u306D";
    post += "#weekend";

    var re = /#weekend/u;
    var n = 500000;
    var result = 0;
    for (var i = 0; i < n; i++) {
        if (re.test(post))
            result++;
    }
    if (result !== n)
        throw "Error: bad result: " + result;
})();
