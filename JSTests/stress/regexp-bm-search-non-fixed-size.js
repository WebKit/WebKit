function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let regexp = /<script.*\/>/i;
let regexpFail = /<scripp.*\/>/i;

for (var i = 0; i < 1e2; ++i) {
    let matched = `aaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbcccccccpppppppppppptttttttttttt<<<<<<<<<<<<<<ddddddddddddddddddddddddddddddddddddddddjjjjjjjjjj<script src="hey.js" />Hey`.match(regexp);
    shouldBe(matched[0], `<script src="hey.js" />`);
    let notMatched = `aaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbcccccccpppppppppppptttttttttttt<<<<<<<<<<<<<<ddddddddddddddddddddddddddddddddddddddddjjjjjjjjjj<script src="hey.js" />Hey`.match(regexpFail);
    shouldBe(notMatched, null);
}
