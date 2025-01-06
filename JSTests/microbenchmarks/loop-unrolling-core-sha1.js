function core_sha1(x)
{
    let result = 0
    for(var i = 0; i < 10; i = (i * 100 + 1) / 100 + 1)
    {
        for(var j = 0; j < 20; j++)
        {
            if(j < 16) result = 0
            else result = 1
        }
    }

  return result
}
noInline(core_sha1)

for (let i = 0; i < 1000; ++i) {
    let result = core_sha1([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15])
    if (result != 1)
        throw "unexpected result: " + result
}
