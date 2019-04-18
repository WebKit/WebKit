//@ runDefault("--useConcurrentJIT=0")
const s = (10).toLocaleString();
const r= RegExp();

function foo()
{
    s.replace(r, s);
}

for (let i = 0; i < 100; i++) {
    foo();
}
