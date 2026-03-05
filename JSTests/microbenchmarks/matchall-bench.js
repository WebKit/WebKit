const re1 = /test/g;
const str1 = "test1test2test3";
for (let i = 0; i < 1e5; i++) {
    const iter = str1.matchAll(re1);
    iter.next();
}
