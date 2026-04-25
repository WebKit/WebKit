function fill(template, title, completed, checked)
{
    return template.replace("{{title}}", title).replace("{{completed}}", completed).replace("{{checked}}", checked);
}
noInline(fill);

function test()
{
    for (var i = 55; i < 100; ++i) {
        for (var j = 0; j < i; ++j) {
            var template = `<li data-id="${j + 1}" class="{{completed}}"><div class="view"><input class="toggle" type="checkbox" {{checked}}><label>{{title}}</label><button class="destroy"></button></div></li>`;
            fill(template, `Something to do ${j}`, "", "");
        }
    }
}
noInline(test);

for (var i = 0; i < 100; ++i)
    test();
