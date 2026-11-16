(function() {
    var result = 0;
    var n = 1000000;
    let str1 = "1234  Abc123 5678 EOL";
    for (let i = 0; i < n; ++i) {
        let re1 = /([\s\S]+?)Abc123([\s\S]+)EOL/;
        let re2 = /([\s\S]+)ABC123([\s\S]+)EOL/i;
        let re3 = /([\s\S]*?)Abc123([\s\S]+)EOL/;
        let re4 = /([\s\S]*)ABC123([\s\S]+)EOL/i;

        if (re1.test(str1))
            ++result;
        if (re2.test(str1))
            ++result;
        if (re3.test(str1))
            ++result;
        if (re4.test(str1))
            ++result;
    }
    if (result != n * 4)
        throw "Error: bad result: " + result;
})();
