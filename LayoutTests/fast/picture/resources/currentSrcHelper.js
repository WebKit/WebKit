function fileName(src) {
    var array = src.split('/');
    return array[array.length -1];
};

function currentSrcFileName(id) {
    var currentSrc = document.getElementById(id).currentSrc;
    return fileName(currentSrc);
};

function currentSrcFileNameNoParams(id) {
    var currentSrc = document.getElementById(id).currentSrc;
    var name = fileName(currentSrc).split('?');
    return name[0];
};
