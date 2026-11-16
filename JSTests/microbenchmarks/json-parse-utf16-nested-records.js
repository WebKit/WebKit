let seed = 12345;
function next() {
    seed = (seed + 0x9e3779b9) | 0;
    let z = seed;
    z = Math.imul(z ^ (z >>> 16), 0x85ebca6b);
    z = Math.imul(z ^ (z >>> 13), 0xc2b2ae35);
    return (z ^ (z >>> 16)) >>> 0;
}

const cities = ["東京", "大阪", "名古屋", "Zürich", "München"];
const users = [];
for (let i = 0; i < 500; ++i) {
    users.push({
        identifier: next() % 100000,
        displayName: "user–" + (next() % 1000),
        emailAddress: "u" + (next() % 1000) + "@example.com",
        address: {
            city: cities[next() % cities.length],
            postalCode: String(next() % 100000),
            coordinates: { latitude: next() % 90, longitude: next() % 180 },
        },
        preferences: { notifications: (next() & 1) === 0, theme: next() & 1 ? "dark" : "light" },
        createdTimestamp: next() % 1000000,
    });
}
const text = JSON.stringify({ users });

for (let i = 0; i < 300; ++i)
    JSON.parse(text);
