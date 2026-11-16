import { shouldBe } from "./resources/assert.js";
import * as ns from "./namespace-object-typed-array-fast-path.js"
export let length = 42;
export let hello = 44;

let array = new Uint8Array(ns);
shouldBe(array.length, 0);
