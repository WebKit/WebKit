function main() {
    let arr = [1];

    arr.length = 0x100000;
    arr.splice(0, 0x11);

    arr.length = 0xfffffff0;
    arr.splice(0xfffffff0, 0, 1);
}

main();
