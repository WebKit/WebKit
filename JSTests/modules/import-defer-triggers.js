//@ requireOptions("--useImportDefer=1")
import { shouldBe } from "./resources/assert.js";

globalThis.deferTriggerEvaluations ||= [];

import defer * as nsHas from "./import-defer/trigger-has.js";
import defer * as nsKeys from "./import-defer/trigger-keys.js";
import defer * as nsDelete from "./import-defer/trigger-delete.js";
import defer * as nsDefine from "./import-defer/trigger-define.js";
import defer * as nsIndex from "./import-defer/trigger-index.js";

shouldBe(globalThis.deferTriggerEvaluations.length, 0);

shouldBe("foo" in nsHas, true);
shouldBe(globalThis.deferTriggerEvaluations.includes("has"), true);

shouldBe(Object.keys(nsKeys).includes("foo"), true);
shouldBe(globalThis.deferTriggerEvaluations.includes("keys"), true);

shouldBe(Reflect.deleteProperty(nsDelete, "foo"), false);
shouldBe(globalThis.deferTriggerEvaluations.includes("delete"), true);

shouldBe(Reflect.defineProperty(nsDefine, "foo", { value: 2 }), false);
shouldBe(globalThis.deferTriggerEvaluations.includes("define"), true);

shouldBe(nsIndex[0], undefined);
shouldBe(globalThis.deferTriggerEvaluations.includes("index"), true);

shouldBe(globalThis.deferTriggerEvaluations.length, 5);
