import * as assert from "../assert.js";
import { compile } from "./wast-wrapper.js";

function testUnreachableBrOn() {
    compile(`
      (module
        (type $s (struct))
        (func
          unreachable
          (loop
            (br_on_null 1 (ref.null $s))
            drop)))
    `);

    compile(`
      (module
        (type $s (struct))
        (func
          (block
            (br 0)
            (loop
              (br_on_null 1 (ref.null $s))
              drop))))
    `);

    compile(`
      (module
        (type $s (struct))
        (func (result (ref null $s))
          unreachable
          (loop
            (br_on_non_null 1 (ref.null $s))
            drop)))
    `);

    compile(`
      (module
        (type $s (struct))
        (func
          unreachable
          (loop
            (br_on_null 0 (ref.null $s))
            drop)))
    `);

    compile(`
      (module
        (type $s (struct))
        (func
          unreachable
          (loop
            (br_on_cast 1 (ref null $s) (ref $s) (ref.null $s))
            drop)))
    `);

    compile(`
      (module
        (type $s (struct))
        (func
          unreachable
          (loop
            (br_on_cast_fail 1 (ref null $s) (ref $s) (ref.null $s))
            drop)))
    `);

    compile(`
      (module
        (type $s (struct))
        (func
          (block
            unreachable
            (block
              (br_on_cast 2 (ref null $s) (ref $s) (ref.null $s))
              drop))))
    `);

    assert.throws(
        () => compile(`
          (module
            (type $s (struct))
            (func
              unreachable
              (loop
                (br_on_null 2 (ref.null $s))
                drop)))
        `),
        WebAssembly.CompileError,
        "exceeds control stack size"
    );

    assert.throws(
        () => compile(`
          (module
            (type $s (struct))
            (func
              unreachable
              (loop
                (br_on_cast 2 (ref null $s) (ref $s) (ref.null $s))
                drop)))
        `),
        WebAssembly.CompileError,
        "exceeds control stack size"
    );

    assert.throws(
        () => compile(`
          (module
            (func
              unreachable
              (loop
                (br_on_cast 0 anyref externref (ref.null any))
                drop)))
        `),
        WebAssembly.CompileError,
        "target heaptype was not a subtype of source heaptype for br_on_cast"
    );
}

testUnreachableBrOn();
