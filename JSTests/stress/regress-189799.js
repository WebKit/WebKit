function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

(function() {

let callback1;
let callbacks2 = new Array();
let callbacks3 = new Array();

for (
    let i = (callback1 = () => i, 0);
    callbacks2.push(() => i), i < 5;
    callbacks3.push(() => i), i++) {
    i++;
}

shouldBe(callback1(), 0);
shouldBe(callbacks2[0](), 1);
shouldBe(callbacks3[0](), 3);

})();

(function() {

let cb;

for (
    let i = (cb = (() => i), 0);
    i < 10;) {
    i++;
}

shouldBe(cb(), 0);

})();
