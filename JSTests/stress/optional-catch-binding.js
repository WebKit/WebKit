function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function throwException() {
    throw new Error(`Cocoa`);
}

shouldBe(function () {
    try {
        throwException();
    } catch {
        return true;
    }
    return false;
}(), true);

shouldBe(function () {
    var ok = false;
    try {
        throwException();
    } catch {
        ok = true;
        return false;
    } finally {
        return ok;
    }
    return false;
}(), true);

shouldBe(function () {
    let value = 'Cocoa';
    try {
        throwException();
    } catch {
        let value = 'Cappuccino';
        return value;
    }
}(), 'Cappuccino');

shouldBe(function () {
    var value = 'Cocoa';
    try {
        throwException();
    } catch {
        let value = 'Cappuccino';
    }
    return value;
}(), 'Cocoa');
