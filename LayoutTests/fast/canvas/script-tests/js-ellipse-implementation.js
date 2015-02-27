function rad2deg(x) {
    return x * 180 / Math.PI;
}

function ellipseUsingArc(context, x, y, radiusX, radiusY, rotation, startAngle, endAngle, anticlockwise)
{
    var transform = new WebKitCSSMatrix();
    transform = transform.translate(x, y);
    transform = transform.rotate(rad2deg(rotation));
    transform = transform.scale(radiusX, radiusY);

    /*
    Use WebKitCSSMatrix instead of as follows, because using WebKitCSSMatrix computes float values more precisely.
    It is because we don't want to fail pixel comparison due to float precision.
      context.translate(x, y);
      context.rotate(rotation);
      context.scale(radiusX, radiusY);
    */
    context.save();
    context.transform(transform.a, transform.b, transform.c, transform.d, transform.e, transform.f);
    context.arc(0, 0, 1, startAngle, endAngle, anticlockwise);
    context.restore();
}

