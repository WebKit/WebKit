eval(`
//# sourceMappingURL=http://example.com
function shouldBe(a, b) {
  if (a !== b)
    throw new Error();
}
shouldBe(1 + 1, 2);
`);
