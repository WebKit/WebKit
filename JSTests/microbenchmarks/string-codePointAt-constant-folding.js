const str1 = "a".repeat(10000);
const index1 = 9999;

const str2 = "Hello, World";
const index2 = 2;

const str3 = "𠮷野家";
const index3 = 0;

for (let i = 0; i < 1e6; i++) {
  str1.codePointAt(index1); 
  str2.codePointAt(index2);
  str3.codePointAt(index3);
}
