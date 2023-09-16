let num_value_accesses = 0;

const result = {
    get done() {
        return 1;
    },

    get value() {
        num_value_accesses++;
    }
};

const iterator = {
    next: () => result
};

const object = {[Symbol.iterator]: () => iterator};

function opt() {
    for (let value of object) {

    }
}

function main() {
    for (let i = 0; i < 1e6; i++) {
        opt();
    }

    if (num_value_accesses)
        throw new Error("Bad assertion!");
}

main();
