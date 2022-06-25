export let Cocoa = "Cocoa";
export function changeCocoa(value) {
    Cocoa = value;
}

import { Cappuccino as SubDrink, changeCappuccino } from "./drink-2.js"
export { SubDrink, changeCappuccino }
