let c = 0
let a = 0

for (let i = 0; i < 100000; ++i)
{
  let c = (i >= i + 1)
  a = c ? 15 : 20
}

for (let i = 0; i < 100000; ++i)
{
  let c = (i > i + 1)
  a = c ? 15 : 20
}

for (let i = 0; i < 100000; ++i)
{
  let c = (i <= i + 1)
  a = c ? 15 : 20
}

for (let i = 0; i < 100000; ++i)
{
  let c = (i < i + 1)
  a = c ? 15 : 20
}
