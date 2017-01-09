(async function () {
    const { shouldBe, shouldThrow } = await import("./import-tests/should.js");

    {
        let cocoa = await eval(`import("./import-tests/cocoa.js")`);
        shouldBe(cocoa.hello(), 42);
    }

    {
        let cocoa = await (0, eval)(`import("./import-tests/cocoa.js")`);
        shouldBe(cocoa.hello(), 42);
    }

    {
        let cocoa = await eval(`eval('import("./import-tests/cocoa.js")')`);
        shouldBe(cocoa.hello(), 42);
    }

    {
        let cocoa = await ((new Function(`return eval('import("./import-tests/cocoa.js")')`))());
        shouldBe(cocoa.hello(), 42);
    }

    {
        let cocoa = await eval(`(new Function('return import("./import-tests/cocoa.js")'))()`);
        shouldBe(cocoa.hello(), 42);
    }

    {
        let cocoa = await [`import("./import-tests/cocoa.js")`].map(eval)[0];
        shouldBe(cocoa.hello(), 42);
    }
}()).catch((error) => {
    print(String(error));
    abort();
});
