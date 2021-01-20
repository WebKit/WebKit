import Builder from '../Builder.js';
import * as assert from '../assert.js';

// Interesting Unicode corner cases.

const names = [
    "", // Empty string.
    "\u0000", // NUL
    "␀", // SYMBOL FOR NUL
    "\ufeff", // BYTE ORDER MARK
    "\ufffc", // OBJECT REPLACEMENT CHARACTER
    "�", // REPLACEMENT CHARACTER in case anything gets substituted this would be a duplicate, which WebAssembly disallows.
    "\ufffe", // NONCHARACTER-FFFE
    "\uffff", // NONCHARACTER-FFFF
    "\u200e", // LEFT-TO-RIGHT MARK
    "\u200f", // RIGHT-TO-LEFT MARK
    "\u202a", // LEFT-TO-RIGHT EMBEDDING
    "\u202b", // RIGHT-TO-LEFT EMBEDDING
    "\u202d", // LEFT-TO-RIGHT OVERRIDE
    "\u202e", // RIGHT-TO-LEFT OVERRIDE
    "\u202c", // POP DIRECTIONAL FORMATTING
    "🇨🇦", // REGIONAL INDICATOR SYMBOL LETTER C + REGIONAL INDICATOR SYMBOL LETTER A
    "🈹", // PLAYING CARD BLACK JOKER
    "👨‍❤️‍💋‍👨", // Combine a bunch of emoji

    // Duplicate entries are invalid, so normalization would cause issues. We want to catch those issues.
    "한글",
    "한글", // Normalized version of the above.
    "Å", // LATIN CAPITAL LETTER A WITH RING ABOVE
    "Å", // ANGSTROM SIGN
    "Å", // LATIN CAPITAL LETTER A + COMBINING RING ABOVE
    "À", // LATIN CAPITAL LETTER A + COMBINING GRAVE ACCENT
    "À", // LATIN CAPITAL LETTER A WITH GRAVE
    "ą̂́", // LATIN SMALL LETTER A WITH CIRCUMFLEX AND ACUTE + COMBINING OGONEK
    "ą̂́", // LATIN SMALL LETTER A WITH OGONEK + COMBINING CIRCUMFLEX ACCENT + COMBINING ACUTE ACCENT
    "Q̣̀", // LATIN CAPITAL LETTER Q + COMBINING GRAVE ACCENT + COMBINING DOT BELOW
    "Q̣̀", // LATIN CAPITAL LETTER + COMBINING DOT BELOW + COMBINING GRAVE ACCENT

    // Combine way too many things:



    "Ĵ̸̨̛͙̙̱̯̺̖͔͎̣͓̬͉͓͙͔͈̥̖͎ͯ́̂͆̓̂̑ͥͧͤ͋̔ͥà̷̛̤̻̦͓͓̖̪̟͓͂ͫ̅ͦͦ̈̄̉̅̾̊̽͑ͬ̚͟͠v̡͙͕̟͎̰͆̿ͩ͗ͩ̀ͧ͂͆̾́͑ͦͭ̽ͦ̀͘a̧̻͖͓̗̻̩͌ͮ͗̈́͊̊̑̉̚̕S̡͖̱͈͚͉̩̺͎ͪͧ́͒̔̿̈ͫͥ̇ͦ̏͐̃ͭ́ͭ͢͝ç̧͎̮̤̙̗̻̯͖̮̙̰̞ͫ̽ͤ̓̊̇͆̀ͫ͂̒͐ͬ̇͂̉͒ͫ̚͘͞ͅṙ̷͓̹͈̮̪͔͓̫̓͛̅̑̉̏͂ͣ́̏͋́̓̚ͅȉͪͩ̍̽ͣ̊ͮ̚͞͏̴̵̯̹̠͖͈̠͎̦̀p͙̝̗̥̻̯̰͚̫̏̏ͯ͐ͧ͋̊̃̈̈́͢t̷͍͎̻̩̠̬̙̦̰͓̩̗̝̱̣̓̐ͫ̀̋̓͝",




];

// WebAssembly import objects have a two-level namespace with module and field. Both must be UTF-8.
let importObject = {};
for (let idxModule = 0; idxModule < names.length; ++idxModule) {
    assert.falsy(importObject.hasOwnProperty(names[idxModule]));
    importObject[names[idxModule]] = {};
    for (let idxField = 0; idxField < names.length; ++idxField) {
        assert.falsy(importObject[names[idxModule]].hasOwnProperty(names[idxField]));
        importObject[names[idxModule]][names[idxField]] = () => 10000 * idxModule + idxField;
    }
}

let b = (new Builder())
    .Type().End();

b = b.Import();
for (let idxModule = 0; idxModule < names.length; ++idxModule)
    for (let idxField = 0; idxField < names.length; ++idxField)
        b = b.Function(names[idxModule], names[idxField], { params: [], ret: "i32" });
b = b.End();

b = b.Function().End();

b = b.Export();
for (let idx = 0; idx < names.length; ++idx)
    b = b.Function(names[idx]);
b = b.End();

b = b.Code();
for (let idx = 0; idx < names.length; ++idx)
    b = b.Function(names[idx], { params: [], ret: "i32" }).I32Const(idx).Return().End();
b = b.End();

const module = new WebAssembly.Module(b.WebAssembly().get());

for (let idxModule = 0; idxModule < names.length; ++idxModule)   
    for (let idxField = 0; idxField < names.length; ++idxField) {
        assert.eq(WebAssembly.Module.imports(module)[idxModule * names.length + idxField].module, names[idxModule]);
        assert.eq(WebAssembly.Module.imports(module)[idxModule * names.length + idxField].name, names[idxField]);
    }

for (let idx = 0; idx < names.length; ++idx)
    assert.eq(WebAssembly.Module.exports(module)[idx].name, names[idx]);

const instance = new WebAssembly.Instance(module, importObject);

for (let idx = 0; idx < names.length; ++idx)
    assert.eq(instance.exports[names[idx]](), idx);
