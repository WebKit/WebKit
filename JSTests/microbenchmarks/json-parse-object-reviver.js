const json = JSON.stringify({a:0,b:1,c:2,d:3,e:4,f:5,g:6,h:7,i:8,j:9});
for (let i = 0; i < 1e5; ++i)
    JSON.parse(json, (_key, val) => val + 1);
