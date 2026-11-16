function C() {}
C.prototype.toJSON = () => '';
const value = {};
for (let i = 0; i < 100; ++i)
    value["k" + i] = new C;
for (let i = 0; i < 1e5 / 4; ++i)
    JSON.stringify(value);
