var arr = [4, 5, 6];
arr.push(10);
arr.pop();
Object.defineProperty(arr, "foo", { });

arr.shift();
arr.splice(0, 0, 101, 102);
Object.defineProperty(arr, "bar", { });
