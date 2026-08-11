let specialIterable = {}

function makeWeirdIterator() {
    let i = {}
    Object.defineProperty(i, "done", { get: WeakMap })
    i.next = () => i
    return i
}

function main() {
    Object.defineProperty(specialIterable, Symbol.iterator, { value: makeWeirdIterator })
    new Set(specialIterable)
}
noDFG(main)
noFTL(main)
try { main() } catch (_) {}
