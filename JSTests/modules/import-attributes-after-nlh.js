// Checks that newlines are allowed before 'with' in 'import' and 'export'

import { shouldBe } from "./resources/assert.js";

import './resources/x.js'
with {};

import x from './resources/x.js'
with {};
shouldBe(x, "x");

import { y } from './resources/y.js'
with {};
shouldBe(y, "y");

export * from './resources/x.js'
with {};

export {} from './resources/x.js'
with {};
