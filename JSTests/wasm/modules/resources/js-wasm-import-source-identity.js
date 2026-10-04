//@ requireOptions("--useSourcePhaseImports=true")
import * as mod1 from "../constant.wasm"
import * as mod2 from "../constant.wasm"
import source mod3 from "../constant.wasm"
import source mod4 from "../constant.wasm"
export { mod1, mod2, mod3, mod4 }
