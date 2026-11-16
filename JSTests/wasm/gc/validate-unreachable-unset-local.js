import { instantiate } from "./wast-wrapper.js";

const wat = `
(module
  (func $test (export "test") (result i32)
    (local $x (ref i31))
    (if (result i32)
      (i32.const 0)
      (then
        unreachable
        (local.set $x (ref.i31 (i32.const 42)))
        (i31.get_u (local.get $x))
      )
      (else
        (i31.get_u (local.get $x))
      )
    )
  )
)
`;

let caughtError;
try {
  await instantiate(wat);
} catch (e) {
  caughtError = e;
}
if (caughtError == undefined ||
    caughtError.constructor !== WebAssembly.CompileError) {
    throw new Error("Expected validation error");
}
