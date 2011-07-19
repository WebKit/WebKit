document.getElementById("description").innerHTML = "Tests that we support cloning deep(ish) arrays.";
deepArray=[];
for (var i = 0; i < 10000; i++)
    deepArray=[deepArray];
tryPostMessage('deepArray');
tryPostMessage('"done"');
