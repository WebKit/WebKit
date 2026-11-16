import func from "./export-default-function-name-in-generator-declaration.js"
import { shouldBe } from "./resources/assert.js";

export default function * () { }

// https://tc39.github.io/ecma262/#sec-exports-runtime-semantics-evaluation
shouldBe(func.name, 'default');
// shouldBe(func.toString(), `function * () { }`);
