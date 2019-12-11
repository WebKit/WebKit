import { compile } from "../wabt-wrapper.js";

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
  (type (func (result ${types.join(" ")})))
  (func $callee (import "env" "callee") (type 0))
  (table funcref (elem $callee))
  (func (export "caller")
    (block
      (call_indirect (type 0) (i32.const 0))
      ${callerChecks}
      return
    )
    unreachable
  )
)
`;
    let module = compile(wat);

    let error;
    function throwError() {
        error = new TypeError();
        throw error;
    }


    let calleeInst = new WebAssembly.Instance(module, { env: { callee: throwError } } );
    for (let i = 0; i < 10000; ++i) {
        try {
            calleeInst.exports.caller();
        } catch (e) {
            if (error !== e)
                throw new Error("wanted error" + error + " but got " + e.message);
        }
    }

    function* throwErrorInIterator() {
        yield 2;
        error = new TypeError();
        throw error;
        return 2;
    }

    calleeInst = new WebAssembly.Instance(module, { env: { callee: throwErrorInIterator } } );
    for (let i = 0; i < 10000; ++i) {
        try {
            calleeInst.exports.caller();
        } catch (e) {
            if (error !== e)
                throw new Error();
        }
    }

    function tooManyValues() {
        let result = types.map((value, index) => index);
        result.push(1);
        return result;
    }

    calleeInst = new WebAssembly.Instance(module, { env: { callee: tooManyValues } } );
    for (let i = 0; i < 10000; ++i) {
        try {
            calleeInst.exports.caller();
        } catch (e) {
            if (!(e instanceof TypeError))
                throw new Error();
        }
    }

    function tooFewValues() {
        let result = types.map((value, index) => index);
        result.shift();
        return result;
    }

    calleeInst = new WebAssembly.Instance(module, { env: { callee: tooFewValues } } );
    for (let i = 0; i < 10000; ++i) {
        try {
            calleeInst.exports.caller();
        } catch (e) {
            if (!(e instanceof TypeError))
                throw new Error();
        }
    }
};

buildWat(["i32"]);
buildWat(["i32", "i32", "f32", "i32"]);
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
