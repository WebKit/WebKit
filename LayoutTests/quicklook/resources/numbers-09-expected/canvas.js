function bezierPath(ctxt, pathString)
{
    var pathTokens = pathString.split(' ');
    var numTokens = pathTokens.length;
    for (i = 0; i < numTokens; i++)
    {
        var op = pathTokens[i];
        
        switch (op)
        {
            case 'm':
            case 'M':
            {
                i++;
                var mx = pathTokens[i++];
                var my = pathTokens[i];
                
                ctxt.moveTo(mx, my);
                break;
            }
                
            case 'l':
            case 'L':
            {
                i++;
                var lx = pathTokens[i++];
                var ly = pathTokens[i];
                
                ctxt.lineTo(lx, ly);
                break;
            }
                
            case 'c':
            case 'C':
            {
                i++;
                var cp1x = pathTokens[i++];
                var cp1y = pathTokens[i++];
                
                var cp2x = pathTokens[i++];
                var cp2y = pathTokens[i++];
                
                var cx = pathTokens[i++];
                var cy = pathTokens[i];
                
                ctxt.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, cx, cy);
                break;
            }
                
            case 'e':
            case 'E':
                break;
                
            case 'z':
            case 'Z':
            {
                ctxt.closePath();
                break;
            }
                
            default:
                break;
        }
    }
}

function rgbaColorString(r, g, b, a)
{
    var redNum = new Number(r);
    var greenNum = new Number(g);
    var blueNum = new Number(b);
    var alphaNum = new Number(a);
    
    var rgbString = 'rgba(' + redNum.toString() + ',' + greenNum.toString() + ',' + blueNum.toString() + ',' + alphaNum.toString() + ')';
    return rgbString;
}

function setColorFillStyle(ctxt, r, g, b, a)
{
    ctxt.fillStyle = rgbaColorString(r, g, b, a);
}

function createLinearGradientFillStyle(ctxt, angle, width, height)
{
    angle = angle + Math.PI;
    var c = Math.cos(angle);
    var s = Math.sin(angle);
    var f = 1.0 / Math.max(Math.abs(s), Math.abs(c));
    var x1 = f * c;
    var y1 = f * s;
    var x0 = -x1;
    var y0 = -y1;
    var halfWidth = width / 2;
    var halfHeight = height / 2;
    x1 = (x1 + 1.0) * halfWidth
    y1 = (y1 + 1.0) * halfHeight;
    x0 = (x0 + 1.0) * halfWidth;
    y0 = (y0 + 1.0) * halfHeight;
    
    ctxt.fillStyle = ctxt.createLinearGradient(x0, y0, x1, y1);
}

function setGradientColorStop(gradient, r, g, b, a, stop)
{
    gradient.addColorStop(stop, rgbaColorString(r, g, b, a));
}

function setColorStrokeStyle(ctxt, r, g, b, a, width)
{
    ctxt.strokeStyle = rgbaColorString(r, g, b, a);
    ctxt.lineWidth = width;
}
