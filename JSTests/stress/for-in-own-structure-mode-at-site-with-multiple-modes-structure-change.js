function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function plain() {
    return { a: 1, b: 2, c: 3 };
}

function withDeletedProperty() {
    const object = { a: 1, x: 0, b: 2, c: 3 };
    delete object.x;
    return object;
}

const prototype = { p: 4, q: 5 };

function withInheritedProperties() {
    const object = Object.create(prototype);
    object.a = 1;
    object.b = 2;
    return object;
}

function deleteWhileEnumerating(object) {
    let result = "";
    for (const key in object) {
        result += key;
        if (key === "a")
            delete object.b;
    }
    return result;
}
noInline(deleteWhileEnumerating);

function addWhileEnumerating(object) {
    let result = "";
    for (const key in object) {
        result += key;
        if (key === "a")
            object.z = 0;
    }
    return result;
}
noInline(addWhileEnumerating);

function addAndDeleteWhileEnumerating(object) {
    let result = "";
    for (const key in object) {
        result += key;
        if (key === "a") {
            object.z = 0;
            delete object.z;
        }
    }
    return result;
}
noInline(addAndDeleteWhileEnumerating);

function hideWhileEnumerating(object) {
    let result = "";
    for (const key in object) {
        result += key;
        if (key === "a")
            Object.defineProperty(object, "b", { enumerable: false });
    }
    return result;
}
noInline(hideWhileEnumerating);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(deleteWhileEnumerating(plain()), "ac");
    shouldBe(deleteWhileEnumerating(withDeletedProperty()), "ac");
    shouldBe(deleteWhileEnumerating(withInheritedProperties()), "apq");
    shouldBe(addWhileEnumerating(plain()), "abc");
    shouldBe(addWhileEnumerating(withDeletedProperty()), "abc");
    shouldBe(addWhileEnumerating(withInheritedProperties()), "abpq");
    shouldBe(addAndDeleteWhileEnumerating(plain()), "abc");
    shouldBe(addAndDeleteWhileEnumerating(withDeletedProperty()), "abc");
    shouldBe(addAndDeleteWhileEnumerating(withInheritedProperties()), "abpq");
    shouldBe(hideWhileEnumerating(plain()), "ac");
    shouldBe(hideWhileEnumerating(withDeletedProperty()), "ac");
    shouldBe(hideWhileEnumerating(withInheritedProperties()), "apq");
}
