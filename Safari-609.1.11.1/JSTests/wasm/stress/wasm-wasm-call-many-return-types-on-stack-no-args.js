import { instantiate } from "../wabt-wrapper.js";

function buildWat(types) {
    let calleeBody = "";
    let callerChecks = [];
    for (let i = 0; i < types.length; ++i) {
        let type = types[i];
        calleeBody += `(${type}.const ${i})`;
        callerChecks.push(`(${type}.ne (${type}.const ${i})) (br_if 0)`);
    }

    callerChecks = callerChecks.reverse().join(" ");
    let wat = `
(module
  (func $callee (result ${types.join(" ")})
    ${calleeBody}
  )
  (func (export "caller")
    (block
      call $callee
      ${callerChecks}
      return
    )
    unreachable
  )
)
`;
    instantiate(wat).exports.caller();
};

buildWat(["i32"]);
buildWat(["i32", "i64", "f32", "i32"]);
buildWat(["i32", "i64", "f32", "i32", "f64", "f32", "i64", "i32"]);

// gpr in registers but fpr spilled. arm64 has 32 fprs so go above that
buildWat(["i64", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i64", "f32", "f32", "f64"]);
// gpr first and in middle
buildWat(["i64", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i64", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32" , "f32", "f32", "f32", "f64"]);

// gpr at end and in middle
buildWat(["f64", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i64", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "i32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32" , "f32", "f32", "f32", "i64"]);
buildWat(["f32", "f32", "f32", "f32", "f32", "f32", "f32", "f32", "i64", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32", "f64", "f32", "i32", "f32", "f32", "f32", "f64", "f32", "f32", "f32", "f32" , "f32", "f32", "f32", "i64"]);


// fpr in registers but gpr spilled. arm64 has 32 fprs so go above that
buildWat(["f64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f64", "i32", "i32", "i64"]);
// fpr first and in middle
buildWat(["f64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f64", "i32", "i32", "i64", "i32", "i32", "i32", "i32", "i32", "i32", "i64", "i32", "i32", "i32", "i32", "i64", "i32", "i32", "i32", "i32", "i64", "i32", "i32", "i32", "i32" , "i32", "i32", "i32", "i64"]);

// gpr at end and in middle
buildWat(["i64", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "f64", "i32", "i32", "i64", "i32", "i32", "i32", "i32", "i32", "i32", "i64", "i32", "i32", "i32", "i32", "i64", "i32", "f32", "i32", "i32", "i32", "i64", "i32", "i32", "i32", "i32" , "i32", "i32", "i32", "f64"]);


// both are spilled
buildWat(["i32", "f32", "i32", "f64", "i64", "f32", "i32", "i32", "f64", "i64", "i32", "f64", "f32", "f32", "f32", "f64", "f32", "i32", "i32", "f32", "i64", "f32", "f64", "f64", "i32", "f32", "f32", "f64", "i64", "i64", "i32", "f64", "f64", "f64", "f32", "f32", "i32", "i64", "i32", "i64", "f32", "f32", "f64", "i64", "i32", "i64", "i64", "i64", "i32", "i32", "f64", "f32", "f32", "f32", "f64", "i32", "i64", "i64", "f32", "f64", "f64", "i32", "i64", "f64", "f64", "f64", "i32", "i64", "i64", "i64"])
