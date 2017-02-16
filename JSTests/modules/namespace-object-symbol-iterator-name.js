import { shouldBe } from "./resources/assert.js";
import * as ns from "./namespace-object-symbol-iterator-name.js";

shouldBe(ns[Symbol.iterator], undefined);
