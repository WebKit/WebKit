const array = [];
for (let i = 0; i < 100; ++i)
    array.push(new Array(1e4).join('—' + i).slice(1));
