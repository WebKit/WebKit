"use strict";

var tf = _interopRequireWildcard(tfjsRequire("@tensorflow/tfjs"));
var use = _interopRequireWildcard(tfjsRequire("@tensorflow-models/universal-sentence-encoder"));
var cocoSsd = _interopRequireWildcard(tfjsRequire("@tensorflow-models/coco-ssd"));
var _tfjsBackendWasm = tfjsRequire("@tensorflow/tfjs-backend-wasm");
var _mobilenetInput = tfjsRequire("./mobilenet-input");

// FIXME: This is speculative fix for jetsams on iOS devices.
// It seems that loading model in each iteration increases
// memory usage on both Safari and Chrome.
globalThis.models = {
	use: null,
	cocoSsd: null,
	knn: null,
	mobilenet_v1: null,
	mobilenet_v3: null
};

function _getRequireWildcardCache(nodeInterop) {
    if (typeof WeakMap !== "function")
        return null;
    var cacheBabelInterop = new WeakMap();
    var cacheNodeInterop = new WeakMap();
    return (_getRequireWildcardCache = function (nodeInterop) {
        return nodeInterop ? cacheNodeInterop : cacheBabelInterop;
    }
    )(nodeInterop);
}

function _interopRequireWildcard(obj, nodeInterop) {
    if (!nodeInterop && obj && obj.__esModule) {
        return obj;
    }
    if (obj === null || typeof obj !== "object" && typeof obj !== "function") {
        return {
            default: obj
        };
    }
    var cache = _getRequireWildcardCache(nodeInterop);
    if (cache && cache.has(obj)) {
        return cache.get(obj);
    }
    var newObj = {};
    var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor;
    for (var key in obj) {
        if (key !== "default" && Object.prototype.hasOwnProperty.call(obj, key)) {
            var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null;
            if (desc && (desc.get || desc.set)) {
                Object.defineProperty(newObj, key, desc);
            } else {
                newObj[key] = obj[key];
            }
        }
    }
    newObj.default = obj;
    if (cache) {
        cache.set(obj, newObj);
    }
    return newObj;
}

const WASM_TFJS_DIR = './wasm/';
const loadAndPredict_mobilenet = async () => {
    // -------------- Load pre-trained mobilenet model --------------
    if (!models.mobilenet_v3) {
        // https://tfhub.dev/google/tfjs-model/imagenet/mobilenet_v2_100_224/classification/3/default/1
        models.mobilenet_v3 = await tf.loadGraphModel('wasm-tfjs-mobilenet', { fromTFHub: true });
        globalThis.MOBILENET_ARTIFACTS_V3 = null;
    }
    const model = models.mobilenet_v3;

    // -------------- Load image data --------------
    const imageSize = 224;
    const imageObject = {
        data: _mobilenetInput.mobilenetInputData,
        width: imageSize,
        height: imageSize
    };

    // -------------- Prediction --------------
    const startTime = performance.now();

    model.predict(tf.zeros([1, imageSize, imageSize, 3])).dispose();
    tf.tidy(() => {
        const img = tf.cast(tf.browser.fromPixels(imageObject), 'float32');
        const offset = tf.scalar(127.5);
        const normalized = img.sub(offset).div(offset);
        const batched = normalized.reshape([1, imageSize, imageSize, 3]);
        return model.predict(batched);
    }
    );

    const totalTime = Math.max(1, performance.now() - startTime);
    return totalTime;
};

const loadAndPredict_knn = async () => {
    // -------------- Load pre-trained KNN classifier model --------------
    if (!models.knn) {
		// https://www.npmjs.com/package/@tensorflow-models/mobilenet
		models.mobilenet_v1 = await tfjsRequire('@tensorflow-models/mobilenet').load();
		// https://www.npmjs.com/package/@tensorflow-models/knn-classifier
		models.knn = tfjsRequire('@tensorflow-models/knn-classifier').create();
        globalThis.MOBILENET_ARTIFACTS_V1 = null;
	}

	const classifier = models.knn;
	const mobilenet = models.mobilenet_v1;

    // -------------- Load image data --------------
    const imageSize = 224;
    const imageObject = {
        data: _mobilenetInput.mobilenetInputData,
        width: imageSize,
        height: imageSize
    };

    // -------------- Prediction --------------
    const startTime = performance.now();
    tf.cast(tf.browser.fromPixels(imageObject), 'float32');
    const logits0 = mobilenet.infer(tf.browser.fromPixels(imageObject), true);
    classifier.addExample(logits0, 0);
    const logits1 = mobilenet.infer(tf.browser.fromPixels(imageObject), true);
    classifier.addExample(logits1, 1);
    mobilenet.infer(tf.browser.fromPixels(imageObject), true);
    const totalTime = Math.max(1, performance.now() - startTime);
    return totalTime;
};

const loadAndPredict_cocoSsd = async () => {
    // -------------- Load pre-trained cocoSsd model --------------
    // https://www.npmjs.com/package/@tensorflow-models/coco-ssd
    if (!models.cocoSsd) {
		models.cocoSsd = await cocoSsd.load();
        globalThis.COCO_SSD_ARTIFACTS == null;
	}
	const model = models.cocoSsd;

    // -------------- Load image data --------------
    const imageSize = 224;
    const imageObject = {
        data: _mobilenetInput.mobilenetInputData,
        width: imageSize,
        height: imageSize
    };

    // -------------- Prediction --------------
    const startTime = performance.now();
    await model.detect(imageObject);
    const totalTime = Math.max(1, performance.now() - startTime);
    return totalTime;
};

const loadAndPredict_use = async () => {
    // -------------- Load pre-trained universal-sentence-encoder model --------------
    // https://www.npmjs.com/package/@tensorflow-models/universal-sentence-encoder
    if (!models.use) {
		models.use = await use.loadQnA();
        globalThis.USE_ARTIFACTS = null;
        globalThis.USE_VOCAB_JSON = null;
	}
	const model = models.use;

    // -------------- Load text data --------------
    const input = {
        queries: ['How are you feeling today?'],
        responses: ['I\'m not feeling very well.', 'Beijing is the capital of China.', 'You have five fingers on your hand.']
    };

    // -------------- Prediction --------------
    const startTime = performance.now();
    let result = model.embed(input);
    tf.matMul(result['queryEmbedding'], result['responseEmbedding'], false, true).dataSync();
    const totalTime = Math.max(1, performance.now() - startTime);
    return totalTime;
};

globalThis.setWasmBackend = async () => {
    // https://github.com/tensorflow/tfjs/tree/master/tfjs-backend-wasm
    // https://github.com/tensorflow/tfjs-models
    _tfjsBackendWasm.setWasmPaths(WASM_TFJS_DIR, (path) => {
        console.log("Fetching " + path)
    });
    let value = await tf.setBackend('wasm')
    if (!value)
        throw new Error("Should use wasm backend.");

    await loadAndPredict();
};

globalThis.loadAndPredict = async () => {
    let result = 0.0;
    result += await loadAndPredict_mobilenet();
    result += await loadAndPredict_knn();
    result += await loadAndPredict_cocoSsd();
    result += await loadAndPredict_use();
    return result;
};
