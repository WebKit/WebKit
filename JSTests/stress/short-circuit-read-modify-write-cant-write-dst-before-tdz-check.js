let result = eval(`
try {
  function captureV() { return v; }

  v &&= "abc";
  let v = var07;
} catch (e) { 
}
`);
if (result !== undefined)
    throw new Error("Bad result")
