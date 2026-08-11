//@ runDefault("--validateDFGClobberize=1", "--jitPolicyScale=0.0001")
for (let i = 0; i < 1e3; i++) {
    (function () {
        var count = 0;
        for (let x = 0; x != 3; x++) {
            count += 1;
            if (count > 10) break;
        }
        if (count != 3)
            throw new Error("bad!");
    })();
}
