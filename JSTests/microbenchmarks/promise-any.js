const promises = [];
for (let i = 0; i < 12; i++)
  promises.push(Promise.reject(i));

for (var i = 0; i < 1e4; ++i)
    Promise.any(promises);

drainMicrotasks();
