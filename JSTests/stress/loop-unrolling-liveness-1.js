function f(y) {
  let x = 0;
  for (let i = 0; i < 2; i++) {
    if (y) {
      x++;
    }
  }
}
for (let j = 0; j < 9; j++) {
  for (let k = 0; k < 999; k++) {
    f(k);
  }
}
(function(){})();
