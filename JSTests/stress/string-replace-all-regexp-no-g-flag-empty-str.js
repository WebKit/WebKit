function fill(template) {
    try {
        template.replaceAll(/ data-id=".*?"/, '');
    } catch {}
}
noInline(fill);

function test() {
    for (var i = 0; i < 8; ++i) {
        for (var j = 0; j < i; ++j) {
            var template = `<li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                            <li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                            <li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>`;
            fill(template);
        }
    }
}
noInline(test);

test();
