//@ runDefault("--validateOptions=true", "--useConcurrentJIT=false", "--useFTLJIT=false")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let v37;
    let v20 = 129n << 129n;
    const v21 = v20++;

    function v29(v30) {
        switch (v21) {
        default:
            for (let v34 = 1; v34 < 65536; v34++) { }
            break;
        case v30:
            v37 = 1; // should never be reached, however this is executed in baseline
        }
    }

    v29(BigInt(129n));
    v29([1]);
    return v37;
}
noInline(main);
shouldBe(main(), undefined);
