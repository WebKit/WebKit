function assert(b) {
    if (!b)
        throw new Error("Bad");
}

async function* asyncIterator() {
    yield 42;
}

function test1() {
    let p = asyncIterator();
    p.next().then((x) => {
        assert(x.value === 42);
        assert(x.done === false);
    });
    p.__proto__ = {};
    assert(p.next === undefined);
}
test1();

let error = null;
async function test2() {
    let p2 = asyncIterator();
    p2.__proto__ = {};
    try {
        for await (let x of p2) {
            throw new Error("Bad!");
        }
    } 
    catch(e) {
        error = e;
    }
}
test2();
assert(error instanceof TypeError);
assert(error.message === "undefined is not a function (near '...x of p2...')");
