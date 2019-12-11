import { instantiate } from "../wabt-wrapper.js";

instantiate(`
(module
  (func
    (local anyref)
    (local anyref)
  )
)
`);
