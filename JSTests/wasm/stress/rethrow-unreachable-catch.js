import { instantiate, compile } from "../wabt-wrapper.js";

// rethrow's label is a catch even when the try is unreachable.
// https://webassembly.github.io/spec/legacy/exceptions/core/valid.html

const valid = `
(module
  (func (export "f")
    block
      br 0
      try
      catch_all
        rethrow 0
      end
    end))
`;

const nested = `
(module
  (func (export "f")
    try
    catch_all
      block
        br 0
        try
        catch_all
          rethrow 2
        end
      end
    end))
`;

const notACatch = `
(module
  (func
    block
      br 0
      rethrow 0
    end))
`;

const innerBlock = `
(module
  (func
    block
      br 0
      block
        try
        catch_all
          rethrow 1
        end
      end
    end))
`;

async function shouldValidate(wat) {
    let instance = await instantiate(wat, {}, { exceptions: true });
    instance.exports.f();
}

await shouldValidate(valid);
await shouldValidate(nested);

for (let wat of [notACatch, innerBlock]) {
    try {
        await compile(wat, { exceptions: true });
        throw new Error("validated");
    } catch (e) {
        if (e instanceof WebAssembly.CompileError)
            continue;
        throw e;
    }
}
