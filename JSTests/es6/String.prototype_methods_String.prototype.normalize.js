function test() {

return typeof String.prototype.normalize === "function"
  && "c\u0327\u0301".normalize("NFC") === "\u1e09"
  && "\u00A8".normalize("NFKC") === "\u0020\u0308"
  && "\u1e09".normalize("NFD") === "c\u0327\u0301";
      
}

if (!test())
    throw new Error("Test failed");

