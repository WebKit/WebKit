function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function fill(template, title, completed, checked) {
    return template.replaceAll("{{title}}", title)
                   .replaceAll("{{completed}}", completed)
                   .replaceAll("{{checked}}", checked);
}
noInline(fill);

for (var i = 55; i < 60; ++i) {
    for (var j = 0; j < i; ++j) {
        var template = `<li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                        <li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>
                        <li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>`;
        const r = fill(template, `Something to do ${j}`, "", "");
        shouldBe(r, `<li data-id="${j + 1}" class=""><div class="view"><input class="toggle" type="checkbox" ><label>Something to do ${j}</label><button class="destroy"></button></div></li>
                        <li data-id="${j + 1}" class=""><div class="view"><input class="toggle" type="checkbox" ><label>Something to do ${j}</label><button class="destroy"></button></div></li>
                        <li data-id="${j + 1}" class=""><div class="view"><input class="toggle" type="checkbox" ><label>Something to do ${j}</label><button class="destroy"></button></div></li>`);
    }
}

