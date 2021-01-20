//@ skip if $memoryLimited

a = 0;

function allocateLotsOfThings(array) {
    for (let i = 0; i < 1e4; i++)
        array[i] = { next: array[Math.floor(i / 2)] };
}

function test() {
    a = 5;
    for (var i = 0; i < 1e3; i++) {
        allocateLotsOfThings([]);
        edenGC();
        eval("var a = new Int32Array(100);");
    }
}
noInline(test);
noDFG(test);

test();
