// FIXME: Bring back something like the deferGC probability mode.
// https://bugs.webkit.org/show_bug.cgi?id=166627
//@ skip
// //@ runFTLNoCJIT("--deferGCShouldCollectWithProbability=true", "--deferGCProbability=1.0")

function foo(a) {
    return a.push(25);
}

function bar(a) {
    for (let i = 0; i < a.length; i++) {
        a[i] = i;
    }
    return foo(a);
}

noInline(bar);

for (let i = 0; i < 100; i++) {
    let smallArray = [1, 2, 3, 4, 5];
    bar(smallArray);
}

let largeArray = [];
for (let i = 0; i < 10000000; i++)
    largeArray.push(i);
bar(largeArray);
