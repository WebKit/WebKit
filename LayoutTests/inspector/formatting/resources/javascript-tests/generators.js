function*gen(){}
function*gen(){1}
function* gen(){}
function* gen(){1}
function *gen(){}
function *gen(){1}

function*gen(){yield}
function*gen(){yield x}
function*gen(){yield"x"}
function*gen(){yield[x]}
function*gen(){yield foo()}

function *gen(a=1,[b,c],...rest){return yield yield foo("foo")}

// ES2018 - Async Iteration / Async Generators

async function*gen(){}
async function*gen(){1}
async function* gen(){}
async function* gen(){1}
async function *gen(){}
async function *gen(){1}

async function*gen(){yield}
async function*gen(){yield x}
async function*gen(){yield"x"}
async function*gen(){yield[x]}
async function*gen(){yield foo()}

async function *gen(a=1,[b,c],...rest){return yield yield foo("foo")}
