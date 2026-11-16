class Base {
    #field;

    static has(object) {
        return #field in object;
    }
}
noInline(Base.has);

let objects = [];
for (let i = 0; i < 32; ++i) {
    class Sub extends Base {
        constructor() {
            super();
            this["extra" + i] = i;
        }
    }
    objects.push(i & 1 ? { ["extra" + i]: i } : new Sub());
}

let count = 0;
for (let i = 0; i < 5e6; ++i) {
    if (Base.has(objects[i & 31]))
        ++count;
}

if (count !== 2500000)
    throw new Error("bad count: " + count);
