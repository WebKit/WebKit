// Verify sticky RegExp matching.

function assert(b)
{
    if (!b)
        throw new Error("Bad assertion");
}

let re = new RegExp("a|aa", "y");
let s1 = "_aaba";
let m = null;

// First character, '_', shouldn't match.
assert(re.exec(s1) === null);
assert(re.lastIndex === 0);

// Second character, 'a', should match and we'll advance to the next character.
re.lastIndex = 1;
m = re.exec(s1);
assert(m[0] === 'a');
assert(re.lastIndex === 2);

// Third character, 'a', should match and we'll advance to the next character.
m = re.exec(s1);
assert(m[0] === 'a');
assert(re.lastIndex === 3);

// Fourth character, 'b', shouldn't match.
m = re.exec(s1);
assert(m === null);
assert(re.lastIndex === 0);

// Fifth character, 'a', should match and we'll advance past the last character.
re.lastIndex = 4;
m = re.exec(s1);
assert(m[0] === 'a');
assert(re.lastIndex === 5);

// We shoudn't match starting past the last character.
m = re.exec(s1);
assert(m === null);

re = new RegExp("ax|a", "y");
// First character, '_', shouldn't match.
assert(re.exec(s1) === null);
assert(re.lastIndex === 0);

// Second character, 'a', should match and we'll advance to the next character.
re.lastIndex = 1;
m = re.exec(s1);
assert(m[0] === 'a');
assert(re.lastIndex === 2);

// Third character, 'a', should match and we'll advance to the next character.
m = re.exec(s1);
assert(m[0] === 'a');
assert(re.lastIndex === 3);

// Fourth character, 'b', shouldn't match.
m = re.exec(s1);
assert(m === null);
assert(re.lastIndex === 0);

// Fifth character, 'a', should match and we'll advance past the last character.
re.lastIndex = 4;
m = re.exec(s1);
assert(m[0] === 'a');
assert(re.lastIndex === 5);

// We shoudn't match starting past the last character.
m = re.exec(s1);
assert(m === null);

re = new RegExp("aa|a", "y");

re.lastIndex = 0;
// First character, '_', shouldn't match.
assert(re.exec(s1) === null);
assert(re.lastIndex === 0);

// Second and third characters, 'aa', should match and we'll advance past them.
re.lastIndex = 1;
m = re.exec(s1);
assert(m[0] === 'aa');
assert(re.lastIndex === 3);

// Fourth character, 'b', shouldn't match.
m = re.exec(s1);
assert(m === null);
assert(re.lastIndex === 0);

// Fifth character, 'a', should match and we'll advance past the last character.
re.lastIndex = 4;
m = re.exec(s1);
assert(m[0] === 'a');
assert(re.lastIndex === 5);

// We shoudn't match starting past the last character.
m = re.exec(s1);
assert(m === null);
