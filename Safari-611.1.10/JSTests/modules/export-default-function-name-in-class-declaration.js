import cls from "./export-default-function-name-in-class-declaration.js"
import { shouldBe } from "./resources/assert.js";

export default class { }

// https://tc39.github.io/ecma262/#sec-exports-runtime-semantics-evaluation
shouldBe(cls.name, 'default');
shouldBe(cls.toString(), `class { }`);
