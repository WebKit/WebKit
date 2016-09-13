function test(bias) {
    let set = new Set;
    let counter = 0;
    for (let i = 0; i < 50000; i++) {
        ++counter;
        if (!set.size || Math.random() > bias) {
            let key = counter; 
            set.add(key);
        } else {
            let keyToRemove = set[Symbol.iterator]().next().value;
            set.delete(keyToRemove);
        }
    }
}

let start = Date.now();
test(0.45);
test(0.60);
const verbose = false;
if (verbose)
    print(Date.now() - start);
