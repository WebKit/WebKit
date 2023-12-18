if (typeof console != "undefined") print = console.log;

const array = [0, 0];
array.__proto__ = {
    get x() { }
}

function opt() {
    for (const key in array)
        array[key] = 1;
}

function main() {
    for (let i = 0; i < 10000; i++)
        opt();
}

main();
