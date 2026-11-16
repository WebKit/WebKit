//@ runDefault("--watchdog=1000", "--watchdog-exception-ok")

let __v_0 = [];
class __c_0 {
  x = __v_0.push();
}
class __c_1 extends __c_0 {
  constructor() {
      __v_0 = () => {
        try {
            __v_0();
        } catch {}
          super();
      };
      __v_0();
  }
}
try {
    new __c_1();
} catch { }
