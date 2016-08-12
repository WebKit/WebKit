import { Cocoa as Drink, changeCocoa, SubDrink, changeCappuccino } from "./aliasing/drink.js"
import { shouldBe, shouldThrow } from "./resources/assert.js";

shouldBe(Drink, "Cocoa");
shouldBe(SubDrink, "Cappuccino");
shouldThrow(() => {
    Cocoa
}, `ReferenceError: Can't find variable: Cocoa`);

shouldThrow(() => {
    Cappuccino
}, `ReferenceError: Can't find variable: Cappuccino`);

changeCocoa("Mocha");
shouldBe(Drink, "Mocha");

changeCappuccino("Matcha");
shouldBe(SubDrink, "Matcha");
