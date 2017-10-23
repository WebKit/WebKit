var array = [];
for (var i = 0; i < 20; ++i)
    array.push(i * 0.1);

function target(array)
{
    return array.toString();
}
noInline(target);

for (var i = 0; i < 1e5; ++i)
    target(array);
