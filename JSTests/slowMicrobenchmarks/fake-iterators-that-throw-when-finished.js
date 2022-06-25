function assert(b) {
    if (!b)
        throw new Error("Bad");
}

class Numbers {
    constructor(limit = 100) {
        this.limit = limit;
        this.item = 0;
    }

    next() {
        if (this.item >= this.limit)
            throw "done";
        return this.item++;
    }
}

function transpose(I, f) {
    return class Transpose {
        constructor(...args) {
            this.iterator = new I(...args);
        }

        next() {
            return f(this.iterator.next());
        }
    };
}

let EvenNumbers = transpose(Numbers, (x)=>x*2);
function verifyEven(prev, cur) {
    assert(cur.value % 2 === 0);
    assert(!prev.value || prev.value+2 === cur.value);
}

let StringNumbers = transpose(Numbers, (x)=>`${x}`);
function verifyString(_, cur) {
    assert(cur.value === `${cur.value}`);
}

let iterators = [
    [Numbers, function() {}],
    [Numbers, function() {}],
    [StringNumbers, verifyString],
    [EvenNumbers, verifyEven],
    [EvenNumbers, verifyEven],
];

function foo(i) {}
noInline(foo);

function runIterators() {
    for (let [iterator, verify] of iterators) {
        let i = new iterator;
        let prev = {};
        while (true) {
            let cur = {};
            try {
                cur.value = i.next();
                verify(prev, cur);
            } catch(e) {
                if (e !== "done")
                    throw new Error("Bad: " + e);
                break;
            }
            prev = cur;
        }
    }
}

{
    let start = Date.now();
    for (let i = 0; i < 5000; ++i)
        runIterators();
    if (false)
        print(Date.now() - start);
}
