//@ memoryHog!
//@ slow!
//@ $skipModes << "no-llint".to_sym

var source = '(function* generatorFunction() {';

for (var i = 0; i < 32768; ++i)
    source += `var v_${i} = ${i};\n`;

source += 'yield;\n})';

var generatorFunction = eval(source);
var generator = generatorFunction();
generator.next();
