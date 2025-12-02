`template string`
`${1+1}`
`${  1  +  1  }`
`before ${1+1} after`
`before ${`nested ${1+1}`} after`
`before ${`nested ${1+1}`} after`

tag`tagged template string`
tag `tagged template string` 
tag`before ${1+1} after`

`
    before ${ 1 + 1 } after
    before ${ x } after
    before ${ func() } after
    before ${ { a: 1 } } after
    before ${ [ 1 ] } after
    before ${
        y
    } after
`
`
A
B
C
`
let shouldNotHaveNewLinesIntroduceBefore;
