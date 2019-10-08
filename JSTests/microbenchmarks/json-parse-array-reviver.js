const json = JSON.stringify([0,1,2,3,4,5,6,7,8,9]);
for (let i = 0; i < 1e5; ++i)
    JSON.parse(json, (_key, val) => val + 1);
