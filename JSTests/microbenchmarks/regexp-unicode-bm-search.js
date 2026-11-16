(function() {
    var article = "";
    var sentences = [
        "\u6771\u4EAC\u90FD\u306F\u65E5\u672C\u306E\u9996\u90FD\u3067\u3042\u308B\u3002",
        "\u4EBA\u53E3\u306F\u7D041400\u4E07\u4EBA\u3067\u3042\u308B\u3002",
        "\u591A\u304F\u306E\u89B3\u5149\u5BA2\u304C\u8A2A\u308C\u308B\u3002",
        "\u6D45\u8349\u5BFA\u3084\u6E0B\u8C37\u306F\u6709\u540D\u3067\u3042\u308B\u3002",
    ];
    for (var i = 0; i < 50; i++)
        article += sentences[i % sentences.length];
    article += "information";

    var re = /information/u;
    var n = 500000;
    var result = 0;
    for (var i = 0; i < n; i++) {
        if (re.test(article))
            result++;
    }
    if (result !== n)
        throw "Error: bad result: " + result;
})();
