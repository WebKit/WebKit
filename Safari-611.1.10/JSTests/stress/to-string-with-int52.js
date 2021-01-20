function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe((0xfffffffffff).toString(16), `fffffffffff`);
shouldBe((-0xfffffffffff).toString(16), `-fffffffffff`);
shouldBe((0xfffffffffff000).toString(16), `fffffffffff000`);
shouldBe((-0xfffffffffff000).toString(16), `-fffffffffff000`);

shouldBe((0x8000000000000).toString(16), `8000000000000`);
shouldBe((-0x8000000000000).toString(16), `-8000000000000`);
shouldBe((0x8000000000000 - 1).toString(16), `7ffffffffffff`);
shouldBe((-0x8000000000000 + 1).toString(16), `-7ffffffffffff`);
