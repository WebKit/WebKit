var objects = [];
for (var i = 0; i < 100; ++i)
    objects.push({ event: true });

for (var i = 0; i < 1e4; ++i)
    String(objects);
