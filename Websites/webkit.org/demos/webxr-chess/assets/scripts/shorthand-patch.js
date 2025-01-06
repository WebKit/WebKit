import {patchElement, schema, threeParts as THREE} from "./short-hand-standalone.min.js";
import {perlin2, seed} from './thirdparty/noise.js';

const v2 = new THREE.Vector2();
const np = new THREE.Vector3();
const temp = new THREE.Vector3();
const nr = new THREE.Vector3();
const nre = new THREE.Euler(0,0,0,'ZXY');
const nrq = new THREE.Quaternion();
const _scale = new THREE.Vector3(1,1,1);
const _matrix = new THREE.Matrix4();

const modSpecial = (n, m) => (m + n % m) % m;
function mod (value, min, max) {
    let norm = value - min;
    return modSpecial(norm, (max-min)) + min;
}

patchElement('sh-mesh', b => class extends(b) {
    static shortHandSchema = new Map([
        ['octaves', {default: 2, stage: schema.STORED_ON_EL}],
        ['position-offset', {as: 'positionOffset', type: 'Vector3', default: '10.3 20.3 30.3', stage: schema.STORED_ON_EL}],
        ['rotation-offset', {as: 'rotationOffset', type: 'Vector3', default: '10.3 20.3 30.3', stage: schema.STORED_ON_EL}],
        ['position-instance-offset', {as: 'positionInstanceOffset', type: 'Vector3', default: '1.1 1.1 1.1', stage: schema.STORED_ON_EL}],
        ['rotation-instance-offset', {as: 'rotationInstanceOffset', type: 'Vector3', default: '1.1 1.1 1.1', stage: schema.STORED_ON_EL}],
        ['position-variance', {as: 'positionVariance', type: 'Vector3', default: '1 1 1', stage: schema.STORED_ON_EL}],
        ['rotation-variance', {as: 'rotationVariance', type: 'Vector3', default: '7 7 7', stage: schema.STORED_ON_EL}],
        ['velocity', {as: 'velocity', type: 'Vector3', default: '0 -1 0', stage: schema.STORED_ON_EL}],
        ['speed', {default: 1, stage: schema.STORED_ON_EL}],
        ['count', {default: 10, constructorArgument: 2 }],
        ['brownian-motion', {as: 'brownianMotion', default: false, stage: schema.STORED_ON_EL}],
    ]);

    async allAttributesChangedCallback() {
        if ( !this.properties.brownianMotion ) return super.allAttributesChangedCallback();
        const object = new THREE.InstancedMesh(...this.constructorArguments);
        object.frustumCulled = false;
        object.instanceMatrix.setUsage( 35048 ); // DynamicDrawUsage = 35048
        await this.assignPostConstructAttributes(object);
        this.updateObject(object);
        this.#seed(20);
    }

    tick(time, ...args) {
        super.tick(time, ...args);
        if ( !this.properties.brownianMotion ) return;

        const ins = this.object;

        if (ins instanceof Promise) return;

        if (!this.startTime) {
            this.startTime = time;
        }

        temp.copy(this.properties.velocity).multiplyScalar(time/1000);
        for (let i=0;i<ins.count;i++) {
            this.#updateNPNRQ(time - this.startTime, i);
            _matrix.compose(np, nrq, _scale);
            ins.setMatrixAt( i, _matrix );
        }
        ins.instanceMatrix.needsUpdate = true;
    }
    #seed(number) {
        seed(number);
    }
    #fbm(x, y, octave) {
        let p = v2.set(x,y);
        let f = 0.0;
        let w = 0.5;
        for (let i = 0; i < octave; i++) {
            f += w * perlin2(p.x, p.y);
            p.multiplyScalar(2.0);
            w *= 0.5;
        }
        return f;
    }
    #updateNPNRQ(time, extraOffsetMultiplier) {
        np.set(
            this.#fbm(this.properties.positionOffset.x + this.properties.positionInstanceOffset.x * extraOffsetMultiplier, this.properties.speed * time/1000, this.properties.octaves),
            this.#fbm(this.properties.positionOffset.y + this.properties.positionInstanceOffset.y * extraOffsetMultiplier, this.properties.speed * time/1000, this.properties.octaves),
            this.#fbm(this.properties.positionOffset.z + this.properties.positionInstanceOffset.z * extraOffsetMultiplier, this.properties.speed * time/1000, this.properties.octaves)
        );

        nr.set(
            this.#fbm(this.properties.rotationOffset.x + this.properties.rotationInstanceOffset.x * extraOffsetMultiplier, this.properties.speed * time/1000, this.properties.octaves),
            this.#fbm(this.properties.rotationOffset.y + this.properties.rotationInstanceOffset.y * extraOffsetMultiplier, this.properties.speed * time/1000, this.properties.octaves),
            this.#fbm(this.properties.rotationOffset.z + this.properties.rotationInstanceOffset.z * extraOffsetMultiplier, this.properties.speed * time/1000, this.properties.octaves)
        );

        nr.multiply(this.properties.rotationVariance);
        nre.setFromVector3(nr);
        nrq.setFromEuler(nre);
        np.multiply(this.properties.positionVariance).multiplyScalar(2);
        np.add(temp);
        for (const d of ['x', 'y', 'z']) {
            const halfVariance = 0.5 * this.properties.positionVariance[d];
            np[d] = mod(np[d], -halfVariance, halfVariance);
        }
    }
});
