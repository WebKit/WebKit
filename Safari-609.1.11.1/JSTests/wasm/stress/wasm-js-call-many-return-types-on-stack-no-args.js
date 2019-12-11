import { instantiate } from "../wabt-wrapper.js";

function buildWat(types) {
    let callerChecks = [];
    for (let i = 0; i < types.length; ++i) {
        let type = types[i];
        callerChecks.push(`(${type}.ne (${type}.const ${i})) (br_if 0)`);
    }

    callerChecks = callerChecks.reverse().join(" ");
    let wat = `
(module
  (func (import "env" "callee") (result ${types.join(" ")}))
  (func (export "caller")
    (block
      call 0
      ${callerChecks}
      return
    )
    unreachable
  ))
`;
    let callee = () => {
        return types.map((type, index) => index);
    }
    
    let exports = instantiate(wat, { env: { callee } }).exports;
    for (let j = 0; j < 10000; ++j) {
        exports.caller();
    }
};

//buildWat(["i32", "i32", "f32", "i32"]);
buildWat(["i32", "i32", "f32", "i32", "f64", "f32", "i32", "i32"]);

// gpr in registers but fpr spilled. arm64 has 32 fprs so go above that
buildWat(["i32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i32", "f32", "f32", "f64"]);
// gpr first and in middle
buildWat(["i32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32" , "f32", "f32", "f32", "f64"]);

// gpr at end and in middle
buildWat(["f64", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "i32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32" , "f32", "f32", "f32", "i32"]);
buildWat(["f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "i32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32" , "f32", "f32", "f32", "i32"]);


// fpr in registers but gpr spilled. arm64 has 32 fprs so go above that
buildWat(["f64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f64", "i32", "i32", "i32"]);
// fpr first and in middle
buildWat(["f64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32" , "i32", "i32", "i32", "i32"]);

// gpr at end and in middle
buildWat(["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32" , "i32", "i32", "i32", "f64"]);


// both are spilled
buildWat(["i32", "f32", "i32", "f64", "i32", "f32", "i32", "i32", "f64", "i32", "i32", "f64", "f32", "f32", "f32", "f64", "f32", "i32", "i32", "f32", "i32", "f32", "f64", "f64", "i32", "f32", "f32", "f64", "i32", "i32", "i32", "f64", "f64", "f64", "f32", "f32", "i32", "i32", "i32", "i32", "f32", "f32", "f64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f64", "f32", "f32", "f32", "f64", "i32", "i32", "i32", "f32", "f64", "f64", "i32", "i32", "f64", "f64", "f64", "i32", "i32", "i32", "i32"])
