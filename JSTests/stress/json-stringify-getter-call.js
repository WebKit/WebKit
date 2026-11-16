function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class A {
    get cocoa() {
        return "Cocoa";
    }

    get cappuccino() {
        return "Cappuccino";
    }
}

let a = new A();
shouldBe(JSON.stringify(a), `{}`);
shouldBe(JSON.stringify(a, ["cocoa", "cappuccino"]), `{"cocoa":"Cocoa","cappuccino":"Cappuccino"}`);

let array = [0, 1, 2, 3, 4];
Object.defineProperty(array.__proto__, 1, {
    get: function () {
        return "Cocoa";
    }
});
Object.defineProperty(array, 0, {
    get: function () {
        delete array[1];
        return "Cappuccino";
    }
});
shouldBe(JSON.stringify(array), `["Cappuccino","Cocoa",2,3,4]`);
