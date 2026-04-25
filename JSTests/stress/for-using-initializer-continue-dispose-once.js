//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    let n = 0, i = 0;
    for (using x = { [Symbol.dispose]() { n++; } }; i < 3; i++) {
        if (i === 1) continue;
    }
    shouldBe(n, 1);
}

{
    let n = 0, i = 0;
    for (using x = { [Symbol.dispose]() { n++; } }; i < 5; i++) {
        continue;
    }
    shouldBe(n, 1);
}

{
    let n = 0, i = 0;
    outer: for (using x = { [Symbol.dispose]() { n++; } }; i < 3; i++) {
        if (i === 1) continue outer;
    }
    shouldBe(n, 1);
}

{
    let outerN = 0, innerN = 0, i = 0;
    for (using x = { [Symbol.dispose]() { outerN++; } }; i < 2; i++) {
        let j = 0;
        for (using y = { [Symbol.dispose]() { innerN++; } }; j < 3; j++) {
            if (j === 1) continue;
        }
    }
    shouldBe(outerN, 1);
    shouldBe(innerN, 2);
}

{
    let outerN = 0, innerN = 0, i = 0;
    outer: for (using x = { [Symbol.dispose]() { outerN++; } }; i < 3; i++) {
        let j = 0;
        for (using y = { [Symbol.dispose]() { innerN++; } }; j < 3; j++) {
            if (j === 1) continue outer;
        }
    }
    shouldBe(outerN, 1);
    shouldBe(innerN, 3);
}

{
    let order = [], i = 0;
    outer: for (using x = { [Symbol.dispose]() { order.push("outer"); } }; i < 3; i++) {
        let j = 0;
        for (using y = { [Symbol.dispose]() { order.push("inner"); } }; j < 3; j++) {
            if (j === 1) break outer;
        }
    }
    shouldBe(order.join(","), "inner,outer");
}

{
    let n = 0, i = 0;
    outer: for (using x = { [Symbol.dispose]() { n++; } }; i < 3; i++) {
        if (i === 1) break outer;
    }
    shouldBe(n, 1);
}

{
    let outerN = 0, innerN = 0, i = 0;
    outer: for (using x = { [Symbol.dispose]() { outerN++; } }; i < 3; i++) {
        for (let j = 0; j < 3; j++) {
            using y = { [Symbol.dispose]() { innerN++; } };
            if (j === 1) break outer;
        }
    }
    shouldBe(outerN, 1);
    shouldBe(innerN, 2);
}

{
    let n = 0, finallyN = 0, i = 0;
    for (using x = { [Symbol.dispose]() { n++; } }; i < 3; i++) {
        try {
            if (i === 1) continue;
        } finally {
            finallyN++;
        }
    }
    shouldBe(n, 1);
    shouldBe(finallyN, 3);
}

{
    let n = 0, i = 0;
    for (using x = { [Symbol.dispose]() { n++; } }; i < 10; i++) {
        if (i === 2) continue;
        if (i === 5) break;
    }
    shouldBe(n, 1);
}

{
    let n = 0, log = [], i = 0;
    for (using x = { [Symbol.dispose]() { n++; } }; i < 3; i++) {
        {
            let inner = i * 10;
            if (i === 1) continue;
            log.push(inner);
        }
    }
    shouldBe(n, 1);
    shouldBe(log.join(","), "0,20");
}

{
    let a = 0, b = 0, i = 0;
    for (using x = { [Symbol.dispose]() { a++; } }, y = { [Symbol.dispose]() { b++; } }; i < 3; i++) {
        if (i === 1) continue;
    }
    shouldBe(a, 1);
    shouldBe(b, 1);
}

{
    let headN = 0, bodyN = 0, i = 0;
    for (using x = { [Symbol.dispose]() { headN++; } }; i < 3; i++) {
        using y = { [Symbol.dispose]() { bodyN++; } };
        if (i === 1) continue;
    }
    shouldBe(headN, 1);
    shouldBe(bodyN, 3);
}

{
    let order = [], i = 0;
    for (using x = { [Symbol.dispose]() { order.push("dispose"); } }; i < 5; i++) {
        order.push("iter" + i);
        if (i === 1) continue;
        if (i === 3) break;
    }
    shouldBe(order.join(","), "iter0,iter1,iter2,iter3,dispose");
}

{
    let finallyN = 0, i = 0;
    for (; i < 3; i++) {
        try {
            if (i === 1) continue;
        } finally {
            finallyN++;
        }
    }
    shouldBe(finallyN, 3);
}

{
    let n = 0, caught, i = 0;
    try {
        for (using x = { [Symbol.dispose]() { n++; throw new Error("E"); } }; i < 3; i++) {
            if (i === 1) continue;
        }
    } catch (e) {
        caught = e;
    }
    shouldBe(n, 1);
    shouldBe(caught.message, "E");
}

async function testAwaitUsing() {
    {
        let n = 0, i = 0;
        for (await using x = { [Symbol.asyncDispose]() { n++; } }; i < 3; i++) {
            if (i === 1) continue;
        }
        shouldBe(n, 1);
    }

    {
        let n = 0, i = 0;
        for (await using x = { [Symbol.asyncDispose]() { n++; } }; i < 5; i++) {
            continue;
        }
        shouldBe(n, 1);
    }

    {
        let outerN = 0, innerN = 0, i = 0;
        outer: for (await using x = { [Symbol.asyncDispose]() { outerN++; } }; i < 3; i++) {
            let j = 0;
            for (await using y = { [Symbol.asyncDispose]() { innerN++; } }; j < 3; j++) {
                if (j === 1) continue outer;
            }
        }
        shouldBe(outerN, 1);
        shouldBe(innerN, 3);
    }
}

let asyncDone = false;
testAwaitUsing().then(() => { asyncDone = true; }, e => { print(e); throw e; });
drainMicrotasks();
shouldBe(asyncDone, true);
