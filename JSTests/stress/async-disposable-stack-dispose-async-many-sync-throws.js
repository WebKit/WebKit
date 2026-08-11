//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

async function test() {
    {
        let called = 0;
        const stack = new AsyncDisposableStack();
        for (let i = 0; i < 30000; i++)
            stack.defer(() => { called++; throw 0; });

        let syncThrew = false;
        let p;
        try {
            p = stack.disposeAsync();
        } catch {
            syncThrew = true;
        }
        shouldBe(syncThrew, false);
        shouldBe(p instanceof Promise, true);

        let caught;
        try { await p; } catch (e) { caught = e; }
        shouldBe(caught instanceof SuppressedError, true);
        shouldBe(called, 30000);
    }

    {
        const stack = new AsyncDisposableStack();
        stack.defer(() => { throw "A"; });
        stack.defer(() => { throw "B"; });
        stack.defer(() => { throw "C"; });

        let caught;
        try { await stack.disposeAsync(); } catch (e) { caught = e; }
        shouldBe(caught.error, "A");
        shouldBe(caught.suppressed.error, "B");
        shouldBe(caught.suppressed.suppressed, "C");
    }

    {
        let log = [];
        const stack = new AsyncDisposableStack();
        stack.defer(() => { log.push(1); throw "S1"; });
        stack.defer(async () => { log.push(2); });
        stack.defer(() => { log.push(3); throw "S2"; });
        stack.defer(async () => { log.push(4); throw "AR"; });
        stack.defer(() => { log.push(5); throw "S3"; });

        let caught;
        try { await stack.disposeAsync(); } catch (e) { caught = e; }
        shouldBe(log.join(","), "5,4,3,2,1");
        shouldBe(caught.error, "S1");
        shouldBe(caught.suppressed.error, "S2");
        shouldBe(caught.suppressed.suppressed.error, "AR");
        shouldBe(caught.suppressed.suppressed.suppressed, "S3");
    }
}

test().then(() => {}, e => { print("FAIL:", e); $vm.abort(); });
drainMicrotasks();
