//@ requireOptions("--useImportAttributes=1")

import { shouldBe } from "./resources/assert.js";
import x from './resources/x.json' with { type: "json", };
shouldBe(JSON.stringify(x), `["x"]`);
