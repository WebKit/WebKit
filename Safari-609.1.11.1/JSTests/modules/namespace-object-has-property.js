import { shouldBe } from "./resources/assert.js";
import * as ns from "./namespace-object-has-property.js"

shouldBe(Reflect.has(ns, 'empty'), true);
shouldBe(Reflect.has(ns, 'undefined'), false);

export let empty;
