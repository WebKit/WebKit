class Base {
    #state;

    constructor(state) {
        this.#state = state;
    }

    getState() {
        return this.#state;
    }
}
noInline(Base.prototype.getState);

let objects = [];
for (let i = 0; i < 32; ++i) {
    class Sub extends Base {
        constructor(state) {
            super(state);
            this["extra" + i] = i;
        }
    }
    objects.push(new Sub(i));
}

let result = 0;
for (let i = 0; i < 5e6; ++i)
    result += objects[i & 31].getState();

if (result !== 77500000)
    throw new Error("bad result: " + result);
