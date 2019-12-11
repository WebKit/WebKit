import drinkCappuccino from './cappuccino.js'
import { shouldBe } from "../resources/assert.js";

shouldBe(drinkCappuccino(), 42);

export default function drinkCocoa()
{
    return 42;
}
