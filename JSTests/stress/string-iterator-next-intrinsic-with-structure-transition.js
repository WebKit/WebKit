// The inlined String Iterator next() puts a tuple node into the same basic block as a
// structure transition. observeTransition() must not treat the tuple node like a normal node.
for (let i = 0; i < 3e5; i++) {
    ""[Symbol.iterator]().next();
    var o = {};
    o.k = i;
}
