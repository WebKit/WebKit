//@ $skipModes << :lockdown if $buildType == "debug"

(function() {
    var result = 0;
    var n = 10000000;
    let src = "foo bar BAZ1 23% 456q Uu X {Xyzz y78} 9  $$$0Abc";
    for (let i = 0; i < n; ++i) {
        let re = /[A-Za-z0-9]{3,4}/g;
        while (re.test(src))
            ++result;
    }
    if (result != n * 7) /* Expect [foo, bar, BAZ1, 456q, Xyzz, y78, 0Abc] */
        throw "Error: bad result: " + result;
})();
