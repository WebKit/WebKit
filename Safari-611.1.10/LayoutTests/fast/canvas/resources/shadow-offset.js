test = function(setter) {
    document.body.appendChild(document.createElement("p")).appendChild(document.createTextNode("There should be no red pixels below."));
    var canvas = document.body.appendChild(document.createElement("canvas"));
    canvas.width = "400";
    canvas.height = "400";
    canvas.style.backgroundColor = "white";
    var ctx = canvas.getContext('2d');
    var w = 10;
    var h = 10;

    var y = 10;
    for (var offsetY = -10; offsetY <= 10; offsetY++) {
        var x = 10;
        for (var offsetX = -10; offsetX <= 10; offsetX++) {
            ctx.setShadow(0, 0, 0, "transparent");
        
            ctx.fillStyle = 'red';
            ctx.fillRect(x + offsetX, y + offsetY, w, h);
    
            setter(ctx, offsetX, offsetY);
        
            ctx.fillStyle = 'white';
            ctx.fillRect(x, y, w, h);

            x += w + Math.abs(offsetX) + 2;
        }
        y += h + Math.abs(offsetY) + 2;
    }
}
