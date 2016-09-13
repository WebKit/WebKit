function assert(b) {
    if (!b)
        throw new Error("Bad!")
}

let set = new Set;
for (let i = 0; i < 64 + ((128 - 64)/2); i++) {
    set.add(i);
}

for (let i = 0; i < 64 + ((128 - 64)/2); i++) {
    set.delete(i);
}
