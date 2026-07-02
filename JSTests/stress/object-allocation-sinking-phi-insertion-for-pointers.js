class Queue {
  _head;
  _tail;
  _length;
  constructor(items) {
    this._head = null;
    this._tail = null;
    this._length = 0;
    if (items) {
      for (const item of items) {
        this.enqueue(item);
      }
    }
  }
  enqueue(item) {
    const entry = {
      next: null,
      value: item,
    };
    if (this._tail) {
      this._tail.next = entry;
      this._tail = entry;
    } else {
      this._head = entry;
      this._tail = entry;
    }
    this._length++;
  }
  dequeue() {
    const entry = this._head;
    if (entry) {
      this._head = entry.next;
      this._length--;
      if (this._head === null) {
        this._tail = null;
      }
      return entry.value;
    } else {
      return null;
    }
  }
}
for (let i = 0; i < testLoopCount; i++) {
  const queue = new Queue(new Set(["foo", "bar", "baz"]));
  if (queue.dequeue() !== "foo") {
    throw new Error("Expected foo");
  }
  if (queue.dequeue() !== "bar") {
    throw new Error("Expected bar");
  }
  if (queue.dequeue() !== "baz") {
    throw new Error("Expected baz");
  }
}
