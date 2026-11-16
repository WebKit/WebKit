//@ $skipModes << :lockdown if $buildType == "debug"

(function() {
    var result = 0;
    var n = 10000000;
    let src = "foo bar BAZ1 23% 456q Uu X {Xyzz y78} 9  $$$0Abc";
    for (let i = 0; i < n; ++i) {
        let re = /[AaCeGgJjMmNnQqTtWwZz]/g;
        while (re.test(src))
            ++result;
    }
    if (result != n * 7) /* Expect [a, A, Z, q, z, z, A] */
        throw "Error: bad result: " + result;
})();
