let x = {};
for (let i=0; i<1000; ++i) {
    gc();
    delete x["h" + 5];
}
