import { t } from "./entry-table.wasm"
export const tableFromJS = new WebAssembly.Table(
    { element: "externref", initial: 10 }
);
export function setTable(index, val) {
    t.set(index, val);
}
