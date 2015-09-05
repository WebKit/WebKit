import Cocoa from "./defaults/Cocoa.js"
import { default as Cappuccino } from "./defaults/Cappuccino.js"
import C2 from "./defaults/Cappuccino.js"
import Matcha from "./defaults/Matcha.js"
import { shouldBe } from "./resources/assert.js";

shouldBe(new Cocoa().taste, 'awesome');
shouldBe(Cappuccino, 'Cappuccino');
shouldBe(C2 === Cappuccino, true);
shouldBe(Matcha, 'Matcha');
