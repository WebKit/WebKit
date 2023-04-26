var obj = {}, i;
for (i = 0; i < 10; ++i)
    obj[`k${i}`] = i;
for (i = 0; i < 10; ++i)
    obj[Symbol(i)] = i;

noInline(Reflect.ownKeys);

for (i = 0; i < 1e5; ++i)
    Reflect.ownKeys(obj);
