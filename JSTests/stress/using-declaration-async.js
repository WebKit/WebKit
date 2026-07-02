//@ requireOptions("--useExplicitResourceManagement=true")
//@ runDefault

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

async function test() {
    {
        let order = [];
        async function f() {
            using x = { [Symbol.dispose]() { order.push("dispose"); } };
            order.push("before-await");
            await Promise.resolve();
            order.push("after-await");
        }
        await f();
        shouldBe(order.join(","), "before-await,after-await,dispose");
    }

    {
        let order = [];
        async function f() {
            using a = { [Symbol.dispose]() { order.push("a"); } };
            await Promise.resolve();
            using b = { [Symbol.dispose]() { order.push("b"); } };
            return "done";
        }
        shouldBe(await f(), "done");
        shouldBe(order.join(","), "b,a");
    }

    {
        let order = [];
        let f = async () => {
            using x = { [Symbol.dispose]() { order.push("arrow-dispose"); } };
            await Promise.resolve();
        };
        await f();
        shouldBe(order.join(","), "arrow-dispose");
    }
}

test().catch(e => {
    print("FAIL: " + e.message);
    $vm.abort();
});
