// Spread(SetObjectUse): the abstract interpreter must resolve the original Set
// structure from the operand's realm, not the Spread node's realm.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

const realmB = createGlobalObject();

realmB.eval(`
    this.BSet = Set;
    this.spreadIt = function (set, o) {
        let x = o.a;
        let arr = [...set];
        return o.a;
    };
    this.installOwnIterator = function (getVictim) {
        Set.prototype[Symbol.iterator] = function* () {
            Object.defineProperty(getVictim(), "a", { get() { return 1; }, configurable: true });
            yield 1;
        };
    };
`);

const BSet = realmB.BSet;
const spreadIt = realmB.spreadIt;

// Pre-create the {a}->Accessor transition so the warmup structure isn't a watchable leaf.
{
    let t = { a: 1.1 };
    Object.defineProperty(t, "a", { get() { return 1; } });
}

let victim = null;

function f(o) {
    // delete() leaves deletedEntryCount != 0, forcing the bail to operationSpreadSet.
    let set = new BSet();
    set.add(1);
    set.add(2);
    set.delete(1);
    return spreadIt(set, o);
}
noInline(f);

for (let i = 0; i < testLoopCount; i++)
    f({ a: 1.1 });

realmB.installOwnIterator(() => victim);

victim = { a: 1.1 };
shouldBe(f(victim), 1);
