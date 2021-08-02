function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let regexp = /ssssss.*ss/;
let regexpFail = /oooooo.*oo/;

for (var i = 0; i < 1e2; ++i) {
    let matched = `aaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbcccccccpppppppppppptttttttttttt<<<<<<<<<<<<<<ddddddddddddddddddddddddddddddddddddddddjjjjjjjjjjssssss src="hey.js" ssHey`.match(regexp);
    shouldBe(matched[0], `ssssss src="hey.js" ss`);
    let notMatched = `aaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbcccccccpppppppppppptttttttttttt<<<<<<<<<<<<<<ddddddddddddddddddddddddddddddddddddddddjjjjjjjjjjssssss src="hey.js" ssHey`.match(regexpFail);
    shouldBe(notMatched, null);
}
