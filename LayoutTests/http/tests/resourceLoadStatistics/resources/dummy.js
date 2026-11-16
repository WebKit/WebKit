function getRandomInt(max) {
    return Math.floor(Math.random() * Math.floor(max));
}
let value = getRandomInt(10);
if (value > 5)
    value = 5;