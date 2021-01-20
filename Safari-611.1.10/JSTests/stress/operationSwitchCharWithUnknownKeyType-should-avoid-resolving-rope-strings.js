//@ if $memoryLimited then skip else runDefault("--useConcurrentJIT=false") end
//@ slow!

var o = (-1).toLocaleString().padEnd(2 ** 31 - 1, "a");

function f() {
    switch (o) {
        case "t":
        case "s":
        case "u":
    }
}
noInline(f);

for (var i = 0; i < 10000; i++)
    f();

