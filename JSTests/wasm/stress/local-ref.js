//@ requireOptions("--useWebAssemblyReferences=1")
import { instantiate } from "../wabt-wrapper.js";

instantiate(`
(module
  (func
    (local anyref)
    (local anyref)
  )
)
`);
