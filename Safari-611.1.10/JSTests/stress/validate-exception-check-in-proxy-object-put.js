const h = {
  set: ()=>1,
};
const t = new String('b');
const p = new Proxy(t, h);
try {
    p[0] = 'a' + 'a';
} catch (e) {
    exception = e;
}

if (exception != "TypeError: Proxy handler's 'set' on a non-configurable and non-writable property on 'target' should either return false or be the same value already on the 'target'")
    throw "FAILED";
