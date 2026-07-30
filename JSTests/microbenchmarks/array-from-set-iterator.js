class ObservableSet {
    constructor(values) {
        this._set = new Set(values);
    }
    keys() {
        return this._set.keys();
    }
}

const set = new ObservableSet();
for (let i = 0; i < 64; ++i)
    set._set.add(i);

let total = 0;
for (let i = 0; i < 1e4; ++i)
    total += Array.from(set.keys()).length;
