var obj = {}, i;
for (i = 0; i < 100; ++i)
    obj[`k${i}`] = i;
for (i = 0; i < 100; ++i)
    obj[Symbol(i)] = i;

noInline(Reflect.ownKeys);

for (i = 0; i < 1e4; ++i)
    Reflect.ownKeys(obj);
