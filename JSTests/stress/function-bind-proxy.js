function f1() {
}
const o2 = {
    "getOwnPropertyDescriptor": f1,
};
const v4 = new Proxy(Date, o2);
const v7 = new Int16Array(v4.bind());
