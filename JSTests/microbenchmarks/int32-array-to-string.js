var array = [];
for (var i = 0; i < 20; ++i)
    array.push(i);

function target(array)
{
    return array.toString();
}
noInline(target);

for (var i = 0; i < 1e5; ++i)
    target(array);
