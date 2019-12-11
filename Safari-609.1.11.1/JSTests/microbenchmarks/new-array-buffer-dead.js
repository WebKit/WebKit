//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo() {
    return [[1,2,3], [5,6,6]];
}

function bar() {
    return ["foo", "bar"];
}

function baz() {
    return [foo(), bar(), foo(), bar()];
}

function thingy() {
    for (var i = 0; i < 1000000; ++i)
        baz();
}

thingy();
