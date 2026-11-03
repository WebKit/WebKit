import Binaryen from "./binaryen/lib.mjs";
// https://emscripten.org/docs/api_reference/module.html#creating-the-module-object
const binaryen = Binaryen({
    print: print,
    printErr: print,
});
export default binaryen;
