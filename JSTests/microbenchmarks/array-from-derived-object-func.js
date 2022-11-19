//@ $skipModes << :lockdown if $buildType == "debug"

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class Derived {
    constructor()
    {
        this.length = 1024 * 1024 * 2;
    }
}

let object = new Derived();
for (let i = 0; i < 30; i++) {
    let array = Array.from(object, () => 42);
    shouldBe(array.length, 1024 * 1024 * 2);
    shouldBe(1 in array, true);
    shouldBe(array[0], 42);
    shouldBe(array[1], 42);
}
