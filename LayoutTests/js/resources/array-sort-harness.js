function log(array) {
  let buf = '';
  for (let index = 0; index < array.length; index++) {
    if (array.hasOwnProperty(index)) {
      buf += String(array[index]);
    } else {
      buf += 'hole';
    }
    if (index < array.length - 1) buf += ',';
  }
  debug(buf);
}
