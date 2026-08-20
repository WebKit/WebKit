// Values that differ per realm live in the metadata of the instruction that needs them rather than in
// the constant pool, which CodeBlock now shares with its UnlinkedCodeBlock.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

// op_get_template_object: a tagged template site must yield the identical frozen array every time it
// is evaluated, and distinct sites must not share one.
function tag(strings) { return strings; }
function site(x) { return tag`a${x}b`; }
function otherSite(x) { return tag`a${x}b`; }

const first = site(0);
shouldBe(Object.isFrozen(first), true);
shouldBe(Object.isFrozen(first.raw), true);
for (let i = 0; i < 1e5; ++i)
    shouldBe(site(i), first);
if (otherSite(0) === first)
    throw new Error("distinct tagged template sites must not share a template object");

function invalidEscape() { return tag`\unicode${0}`; }
shouldBe(invalidEscape()[0], undefined);
shouldBe(invalidEscape().raw[0], "\\unicode");

// op_new_array_buffer: once the allocation profile wants an indexing type the constant does not have,
// the re-typed butterfly is cached in metadata. Fresh allocations must not observe mutations made to
// earlier ones.
function literal() { return [1, 2, 3]; }
for (let i = 0; i < 3e5; ++i) {
    const array = literal();
    shouldBe(array.length, 3);
    shouldBe(`${array[0]}${array[1]}${array[2]}`, "123");
    array[1] = 2.5;
    shouldBe(array[1], 2.5);
}
for (let i = 0; i < 1e3; ++i)
    shouldBe(literal()[1], 2);

// op_create_lexical_environment and op_create_generator_frame_environment take their per-realm
// SymbolTable clone from metadata; op_put_to_scope holds the clone its WatchpointSet points into.
function counter(start) {
    let value = start;
    return { bump() { return ++value; }, read() { return value; } };
}
const counters = [];
for (let i = 0; i < 1e3; ++i)
    counters.push(counter(i));
for (let round = 0; round < 20; ++round) {
    for (const c of counters)
        c.bump();
}
shouldBe(counters[0].read(), 20);
shouldBe(counters[999].read(), 1019);

function blockScopes() {
    const closures = [];
    for (let i = 0; i < 8; ++i) {
        let captured = i * 2;
        closures.push(() => captured);
    }
    return closures.map(f => f()).join(",");
}
for (let i = 0; i < 2e4; ++i)
    shouldBe(blockScopes(), "0,2,4,6,8,10,12,14");

function* generator(n) {
    let sum = 0;
    for (let i = 0; i < n; ++i) {
        sum += i;
        yield sum;
    }
}
for (let i = 0; i < 2e4; ++i) {
    let last = 0;
    for (const value of generator(5))
        last = value;
    shouldBe(last, 10);
}
