import { shouldBe } from "./resources/assert.js";
import drinkCocoa from './export-default-function-hoisting/cocoa.js'

shouldBe(drinkCocoa(), 42);

