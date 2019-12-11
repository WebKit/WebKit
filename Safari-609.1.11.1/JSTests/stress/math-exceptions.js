const foo = new Proxy({}, {
    get() { throw 0xc0defefe; }
});

const bar = new Proxy({}, {
    get() { throw 0xdeadbeef; }
});

const check = value => {
    if (value !== 0xc0defefe)
        throw new Error(`bad ${value}!`);
}

try { Math.acos(foo, bar); } catch (e) { check(e); }
try { Math.acosh(foo, bar); } catch (e) { check(e); }
try { Math.asin(foo, bar); } catch (e) { check(e); }
try { Math.asinh(foo, bar); } catch (e) { check(e); }
try { Math.atan(foo, bar); } catch (e) { check(e); }
try { Math.atanh(foo, bar); } catch (e) { check(e); }
try { Math.atan2(foo, bar); } catch (e) { check(e); }
try { Math.cbrt(foo, bar); } catch (e) { check(e); }
try { Math.ceil(foo, bar); } catch (e) { check(e); }
try { Math.clz32(foo, bar); } catch (e) { check(e); }
try { Math.cos(foo, bar); } catch (e) { check(e); }
try { Math.cosh(foo, bar); } catch (e) { check(e); }
try { Math.exp(foo, bar); } catch (e) { check(e); }
try { Math.expm1(foo, bar); } catch (e) { check(e); }
try { Math.floor(foo, bar); } catch (e) { check(e); }
try { Math.fround(foo, bar); } catch (e) { check(e); }
try { Math.hypot(foo, bar); } catch (e) { check(e); }
try { Math.imul(foo, bar); } catch (e) { check(e); }
try { Math.log(foo, bar); } catch (e) { check(e); }
try { Math.log1p(foo, bar); } catch (e) { check(e); }
try { Math.log10(foo, bar); } catch (e) { check(e); }
try { Math.log2(foo, bar); } catch (e) { check(e); }
try { Math.max(foo, bar); } catch (e) { check(e); }
try { Math.min(foo, bar); } catch (e) { check(e); }
try { Math.pow(foo, bar); } catch (e) { check(e); }
Math.random(foo, bar);
try { Math.round(foo, bar); } catch (e) { check(e); }
try { Math.sign(foo, bar); } catch (e) { check(e); }
try { Math.sin(foo, bar); } catch (e) { check(e); }
try { Math.sinh(foo, bar); } catch (e) { check(e); }
try { Math.sqrt(foo, bar); } catch (e) { check(e); }
try { Math.tan(foo, bar); } catch (e) { check(e); }
try { Math.tanh(foo, bar); } catch (e) { check(e); }
try { Math.trunc(foo, bar); } catch (e) { check(e); }
