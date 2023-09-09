let o = { "$shared": false, "borderWidth": 1, "hitRadius": 1, "hoverBorderWidth": 1, "hoverRadius": 4, "pointStyle": "circle", "radius": 3, "rotation": 0, "backgroundColor": "rgba(0, 125, 255, .20)", "borderColor": "rgba(0,0,0,0.1)", "1000": 1000 };
delete o["1000"];

const cloneIfNotShared = (cached, shared) => {
    shared ? cached : Object.assign({}, cached)
};

for (let i = 0; i < 1e4; i++)
    cloneIfNotShared(o, 0);