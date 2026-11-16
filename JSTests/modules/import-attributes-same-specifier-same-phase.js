import { shouldBe } from "./resources/assert.js";

// A JSON-typed request and an attribute-less (JavaScript) request for the same
// specifier are distinct module requests: both must be loaded, and each binding
// must resolve against the module record matching its request's type.
import jsonValue from "./resources/dual-type.js" with { type: "json" };
import * as jsNs from "./resources/dual-type.js";

shouldBe(jsonValue, 42);
shouldBe(Object.prototype.toString.call(jsNs), "[object Module]");
shouldBe("default" in jsNs, false);
