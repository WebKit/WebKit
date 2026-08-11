const array = [];
for (let i = 0; i < 12; i++)
    array.push(i);

for (var i = 0; i < 1e4; ++i)
    Promise.all(array);

drainMicrotasks();
