function fill(template) {
    return template.replaceAll(/ data-id=".*?"/g, ' replaced-id="value"');
}
noInline(fill);

function sameValue(a, b) {
    if (a !== b) {
        throw new Error(`Expected \n${b}\nbut got \n${a}`);
    }
}

function test(i) {
    for (var i = 55; i < 100; ++i) {
        for (var j = 0; j < i; ++j) {
            var template = `<li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                            <li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                            <li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>`;
            var expected = `<li replaced-id="value" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                            <li replaced-id="value" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                            <li replaced-id="value" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>`;
            var result = fill(template);
            sameValue(result, expected);
        }
    }
}
noInline(test);

for (var i = 0; i < 5; ++i)
    test(i);
