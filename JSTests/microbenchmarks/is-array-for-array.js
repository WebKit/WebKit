function isArray(array)
{
    return Array.isArray(array);
}
noInline(isArray);

for (var i = 0; i < 1e5; ++i)
    isArray([]);
