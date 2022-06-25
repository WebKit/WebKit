//@ if $jitTests then runDefault("--useLLInt=0") else skip end

const o = {};
const handler = {
  get(target, prop, receiver) {
      o.__proto__ = receiver;
  },
  has(target, prop) {
      o.__proto__ = undefined;
      return 1;
  }
};

const p = new Proxy({}, handler);
handler.__proto__ = p;
try {
    with (p) {
        a = 0
    }
    throw new Error("Should throw RangeError");
} catch (error) {
    if (error.message !== "Maximum call stack size exceeded.")
        throw new Error("Expected stack overflow, but got: " + error);
}
