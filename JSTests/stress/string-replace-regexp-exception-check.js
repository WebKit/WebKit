function test() {
  let x = '';
  for (let i = 0; i < 10000; i++) {
    try {
      let y = x.replace(/foo/, '');
      let z = String.fromCharCode(365);
      x = z.padEnd(2147483644, '123' + '');
    } catch (e) {}
  }
}
test();
