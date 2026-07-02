import * as namespace from "./destructuring-export/main.js"
import { array } from "./destructuring-export/array.js"
import { shouldBe } from "./resources/assert.js";

shouldBe(JSON.stringify(Object.getOwnPropertyNames(namespace)), `["Cappuccino","Cocoa","Matcha"]`);
shouldBe(namespace.Cocoa, 'Cocoa');
shouldBe(namespace.Cappuccino, 'Cappuccino');
shouldBe(namespace.Matcha, 'Matcha');

shouldBe(JSON.stringify(array), `[1,2,3,4,5,6,7,8,9]`);
