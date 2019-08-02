//@ runDefault("--useConcurrentJIT=0")
function foo(x) {
    switch (x) {
    case "a":
    case "a":
    case "a":
        for (let j = 0; j <100; j++) {
            let j=foo(j);
        }
    default:
        return 2;
    }
}

for (let i = 0; i <100000; i++) {
    foo("ab");
}
