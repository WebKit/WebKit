
function parseArguments(argv, acceptedOptions) {
    var args = argv.slice(2);
    var options = {}
    for (var i = 0; i < args.length; i += 2) {
        var current = args[i];
        var next = args[i + 1];
        for (var option of acceptedOptions) {
            if (current == option['name']) {
                options[option['name']] = next;
                next = null;
                break;
            }
        }
        if (next) {
            console.error('Invalid argument:', current);
            return null;
        }
    }
    for (var option of acceptedOptions) {
        var name = option['name'];
        if (option['required'] && !(name in options)) {
            console.log('Required argument', name, 'is missing');
            return null;
        }
        var value = options[name] || option['default'];
        var converter = option['type'];
        options[name] = converter ? converter(value) : value;
    }
    return options;
}

if (typeof module != 'undefined')
    module.exports.parseArguments = parseArguments;
