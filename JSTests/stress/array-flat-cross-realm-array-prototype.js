function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

{
    let other = createGlobalObject();
    other.Array.prototype.length = 2;
    other.Array.prototype[0] = "A";
    other.Array.prototype[1] = "B";

    let result = [other.Array.prototype].flat();
    shouldBe(result.length, 2);
    shouldBe(result[0], "A");
    shouldBe(result[1], "B");
}

{
    let other = createGlobalObject();
    other.Array.prototype.length = 3;
    other.Array.prototype[0] = 10;
    other.Array.prototype[1] = 20;
    other.Array.prototype[2] = 30;

    let result = [1, other.Array.prototype, 2].flat();
    shouldBe(result.length, 5);
    shouldBe(result[0], 1);
    shouldBe(result[1], 10);
    shouldBe(result[2], 20);
    shouldBe(result[3], 30);
    shouldBe(result[4], 2);
}

{
    let other = createGlobalObject();
    other.Array.prototype.length = 2;
    other.Array.prototype[0] = "x";
    other.Array.prototype[1] = "y";

    let result = [[other.Array.prototype]].flat(2);
    shouldBe(result.length, 2);
    shouldBe(result[0], "x");
    shouldBe(result[1], "y");
}

{
    let other = createGlobalObject();
    let result = [other.Array.prototype, 42].flat();
    shouldBe(result.length, 1);
    shouldBe(result[0], 42);
}

{
    let other = createGlobalObject();
    other.Array.prototype.length = 2;
    other.Array.prototype[0] = "A";
    other.Array.prototype[1] = "B";

    let result = [other.Array.prototype].flat(0);
    shouldBe(result.length, 1);
    shouldBe(result[0], other.Array.prototype);
}

{
    let other = createGlobalObject();
    shouldBe(Array.isArray(other.Array.prototype), true);
}
