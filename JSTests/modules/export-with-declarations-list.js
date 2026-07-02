import * as namespace from "./export-with-declarations-list/main.js"
import { shouldBe } from "./resources/assert.js";

shouldBe(JSON.stringify(Object.getOwnPropertyNames(namespace)), `["Cappuccino","Cocoa","Matcha","Mocha"]`);
shouldBe(namespace.Cocoa, 'Cocoa');
shouldBe(namespace.Cappuccino, 'Cappuccino');
shouldBe(namespace.Matcha, 'Matcha');
shouldBe(namespace.Mocha, 'Mocha');

