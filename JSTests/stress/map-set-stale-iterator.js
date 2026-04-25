function testStaleIterator(C, setFn) {
  const CONTAINER_SIZE = 1000;

  let container = new C();
  for (let i = 0; i < CONTAINER_SIZE; i++)
    setFn(container, i);

  const it = container.entries();
  while (!it.next().done) {}

  container.clear();

  if (it.toArray().length !== 0)
    throw new Error("toArray on cleared container should be empty");
}

for (let i = 0; i < 10; i++) {
  testStaleIterator(Set, (s, i) => s.add(i));
  testStaleIterator(Map, (m, i) => m.set(i));
}
