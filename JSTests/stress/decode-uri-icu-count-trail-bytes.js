const rest = new Array(14).fill("%00").join('');
const uri = `%fd%f0%f0%f0%ff%ff%ff${rest}`;
for (let i = 0; i < 256; i++) {
    for (let j = 0; j < 256; j++) {
        try {
            decodeURIComponent(`${uri}%${i.toString(16)}%${j.toString(16)}%ff%ff%ff%ff%ff`);
        } catch (err) {
        }
    }
}
