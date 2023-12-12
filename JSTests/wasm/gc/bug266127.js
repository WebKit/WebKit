//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile } from "./wast-wrapper.js";

function testUnreachable()
{
  // A local.set in unreachable code should still count for init.
  compile(`
    (module
      (type (struct))
      (func (local (ref 0))
        (unreachable)
        (local.set 0 (struct.new 0))
        (local.get 0)
        drop))
  `);

  compile(`
    (module
      (type (struct))
      (func (local (ref 0))
        (unreachable)
        (local.tee 0 (struct.new 0))
        drop
        (local.get 0)
        drop))
  `);
}

testUnreachable();
