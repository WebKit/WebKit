function test() {
    var text = '<a href="https\u00a8E58E//developer.nvidia.com/content/depth-precision-visualized" rel="noopener noreferrer" target="\u00a8E95Eblank">https\u00a8E58E//developer.nvidia.com/content/depth-precision-visualized</a>';
    var reg = /<([^>]+?)>[\s\S]*?<\/\1>/g;
    var result = text.replace(reg, function () { return "REPLACED"; });
}

test();
