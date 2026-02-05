// src/iongraph.ts
var currentVersion = 1;
function migrate(ionJSON) {
  if (ionJSON.version === void 0) {
    ionJSON.version = 0;
  }
  for (const f of ionJSON.functions) {
    migrateFunc(f, ionJSON.version);
  }
  ionJSON.version = currentVersion;
  return ionJSON;
}
function migrateFunc(f, version) {
  for (const p of f.passes) {
    for (const b of p.mir.blocks) {
      migrateMIRBlock(b, version);
    }
    for (const b of p.lir.blocks) {
      migrateLIRBlock(b, version);
    }
  }
  return f;
}
function migrateMIRBlock(b, version) {
  if (version === 0) {
    b.ptr = (b.id ?? b.number) + 1;
    b.id = b.number;
  }
  for (const ins of b.instructions) {
    migrateMIRInstruction(ins, version);
  }
  return b;
}
function migrateMIRInstruction(ins, version) {
  if (version === 0) {
    ins.ptr = ins.id;
  }
  return ins;
}
function migrateLIRBlock(b, version) {
  if (version === 0) {
    b.ptr = b.id ?? b.number;
    b.id = b.number;
  }
  for (const ins of b.instructions) {
    migrateLIRInstruction(ins, version);
  }
  return b;
}
function migrateLIRInstruction(ins, version) {
  if (version === 0) {
    ins.ptr = ins.id;
    ins.mirPtr = null;
  }
  return ins;
}

// src/utils.ts
function clamp(x, min, max) {
  return Math.max(min, Math.min(max, x));
}
function filerp(current, target, r, dt) {
  return (current - target) * Math.pow(r, dt) + target;
}
function assert(cond, msg, soft = false) {
  if (!cond) {
    if (soft) {
      console.error(msg ?? "Assertion failed");
    } else {
      throw new Error(msg ?? "Assertion failed");
    }
  }
}
function must(val, msg) {
  assert(val, msg);
  return val;
}

// src/dom.ts
function N(v) {
  if (typeof v === "string") {
    return document.createTextNode(v);
  }
  return v;
}
function addChildren(n, children) {
  for (const child of children) {
    if (child) {
      n.appendChild(N(child));
    }
  }
}
function E(type, classes, init, children) {
  const el = document.createElement(type);
  if (classes && classes.length > 0) {
    const actualClasses = classes.filter((c) => !!c);
    el.classList.add(...actualClasses);
  }
  init?.(el);
  if (children) {
    addChildren(el, children);
  }
  return el;
}

// src/dequal.ts
var has = Object.prototype.hasOwnProperty;
function dequal(foo, bar) {
  var ctor, len;
  if (foo === bar) return true;
  if (foo && bar && (ctor = foo.constructor) === bar.constructor) {
    if (ctor === Date) return foo.getTime() === bar.getTime();
    if (ctor === RegExp) return foo.toString() === bar.toString();
    if (ctor === Array) {
      if ((len = foo.length) === bar.length) {
        while (len-- && dequal(foo[len], bar[len])) ;
      }
      return len === -1;
    }
    if (!ctor || typeof foo === "object") {
      len = 0;
      for (ctor in foo) {
        if (has.call(foo, ctor) && ++len && !has.call(bar, ctor)) return false;
        if (!(ctor in bar) || !dequal(foo[ctor], bar[ctor])) return false;
      }
      return Object.keys(bar).length === len;
    }
  }
  return foo !== foo && bar !== bar;
}

// src/tweak.ts
function tweak(name, initial, options = {}) {
  let value = initial;
  const callbacks = [];
  const t = {
    get() {
      return value;
    },
    set(v) {
      value = v;
      for (const func of callbacks) {
        func(value);
      }
    },
    valueOf() {
      return value;
    },
    toString() {
      return String(value);
    },
    [Symbol.toPrimitive](hint) {
      if (hint === "string") {
        return String(value);
      }
      return value;
    },
    onChange(func) {
      callbacks.push(func);
    },
    initial,
    name,
    min: options.min ?? 0,
    max: options.max ?? 100,
    step: options.step ?? 1
  };
  return (options.tweaksObject ?? globalTweaks).add(t);
}
var Tweaks = class {
  constructor(options) {
    this.container = options.container;
    this.tweaks = [];
    this.callbacks = [];
  }
  /**
   * Adds a Tweak to this object. Not intended to be called directly; instead,
   * just call {@link tweak} with the `tweaksObject` option.
   */
  add(tweak2) {
    const existing = this.tweaks.find((t) => t.name === tweak2.name);
    if (existing) {
      return existing;
    }
    this.tweaks.push(tweak2);
    const el = document.createElement("div");
    this.container.appendChild(el);
    el.style.display = "flex";
    el.style.alignItems = "center";
    el.style.justifyContent = "end";
    el.style.gap = "0.5rem";
    const safename = tweak2.name.replace(/[a-zA-Z0-9]/g, "_");
    const label = document.createElement("label");
    el.appendChild(label);
    label.innerText = tweak2.name;
    label.htmlFor = `tweak-${safename}-input`;
    const input = document.createElement("input");
    el.appendChild(input);
    input.type = "number";
    input.value = String(tweak2);
    input.id = `tweak-${safename}-input`;
    input.style.width = "4rem";
    input.addEventListener("input", () => {
      tweak2.set(input.valueAsNumber);
    });
    const range = document.createElement("input");
    el.appendChild(range);
    range.type = "range";
    range.value = String(tweak2);
    range.min = String(tweak2.min);
    range.max = String(tweak2.max);
    range.step = tweak2.step === 0 ? "any" : String(tweak2.step);
    range.addEventListener("input", () => {
      tweak2.set(range.valueAsNumber);
    });
    const reset = document.createElement("button");
    el.appendChild(reset);
    reset.innerText = "Reset";
    reset.disabled = tweak2.get() === tweak2.initial;
    reset.addEventListener("click", () => {
      tweak2.set(tweak2.initial);
    });
    tweak2.onChange((v) => {
      input.value = String(v);
      range.value = String(v);
      reset.disabled = tweak2.get() === tweak2.initial;
      for (const func of this.callbacks) {
        func(tweak2);
      }
    });
    return tweak2;
  }
  onTweak(func) {
    this.callbacks.push(func);
  }
};
var globalContainer = document.createElement("div");
globalContainer.classList.add("tweaks-panel");
var globalTweaks = new Tweaks({ container: globalContainer });
globalTweaks.onTweak((t) => {
  window.dispatchEvent(new CustomEvent("tweak", { detail: t }));
});
window.tweaks = globalTweaks;
var testContainer = document.createElement("div");
var testTweaks = new Tweaks({ container: testContainer });
var testTweak = tweak("Test Value", 3, { tweaksObject: testTweaks });
testTweak.set(4);
testTweak = 4;

// src/Graph.ts
var DEBUG = tweak("Debug?", 0, { min: 0, max: 1 });
var CONTENT_PADDING = 20;
var BLOCK_GAP = 44;
var PORT_START = 16;
var PORT_SPACING = 60;
var ARROW_RADIUS = 12;
var TRACK_PADDING = 36;
var JOINT_SPACING = 16;
var BACKEDGE_GAP = 40;
var LAYOUT_ITERATIONS = tweak("Layout Iterations", 2, { min: 0, max: 6 });
var NEARLY_STRAIGHT = tweak("Nearly Straight Threshold", 30, { min: 0, max: 200 });
var NEARLY_STRAIGHT_ITERATIONS = tweak("Nearly Straight Iterations", 8, { min: 0, max: 10 });
var STOP_AT_PASS = tweak("Stop At Pass", 30, { min: 0, max: 30 });
var ZOOM_SENSITIVITY = 1.5;
var WHEEL_DELTA_SCALE = 0.01;
var MAX_ZOOM = 1;
var MIN_ZOOM = 0.1;
var TRANSLATION_CLAMP_AMOUNT = 40;
var LEFTMOST_DUMMY = 1 << 0;
var RIGHTMOST_DUMMY = 1 << 1;
var IMMINENT_BACKEDGE_DUMMY = 1 << 2;
var SC_TOTAL = 0;
var SC_SELF = 1;
var log = new Proxy(console, {
  get(target, prop) {
    const field = target[prop];
    if (typeof field !== "function") {
      return field;
    }
    return +DEBUG ? field.bind(target) : () => {
    };
  }
});
var Graph = class {
  constructor(viewport, pass, options = {}) {
    // Layout stabilization (two-pass layout approach)
    this.stabilizationTimeout = null;
    this.layoutFinalized = false;
    // Deferred position restoration
    this.deferredJumpToBlock = null;
    this.viewport = viewport;
    const viewportRect = viewport.getBoundingClientRect();
    this.viewportSize = {
      x: viewportRect.width,
      y: viewportRect.height
    };
    this.graphContainer = document.createElement("div");
    this.graphContainer.classList.add("ig-graph");
    this.graphContainer.style.transformOrigin = "top left";
    this.viewport.appendChild(this.graphContainer);
    this.sampleCounts = options.sampleCounts;
    this.maxSampleCounts = [0, 0];
    this.heatmapMode = SC_TOTAL;
    for (const [ins, count] of this.sampleCounts?.totalLineHits ?? []) {
      this.maxSampleCounts[SC_TOTAL] = Math.max(this.maxSampleCounts[SC_TOTAL], count);
    }
    for (const [ins, count] of this.sampleCounts?.selfLineHits ?? []) {
      this.maxSampleCounts[SC_SELF] = Math.max(this.maxSampleCounts[SC_SELF], count);
    }
    this.size = { x: 0, y: 0 };
    this.numLayers = 0;
    this.zoom = 1;
    this.translation = { x: 0, y: 0 };
    this.animating = false;
    this.targetZoom = 1;
    this.targetTranslation = { x: 0, y: 0 };
    this.startMousePos = { x: 0, y: 0 };
    this.lastMousePos = { x: 0, y: 0 };
    this.selectedBlockPtrs = /* @__PURE__ */ new Set();
    this.lastSelectedBlockPtr = 0;
    this.nav = {
      visited: [],
      currentIndex: -1,
      siblings: []
    };
    this.highlightedInstructions = [];
    this.instructionPalette = options.instructionPalette ?? [0, 1, 2, 3, 4].map((n) => `var(--ig-highlight-${n})`);
    this.blocks = pass.mir.blocks.map((m) => {
      const block = {
        ptr: m.ptr,
        id: m.id,
        mir: m,
        lir: pass.lir.blocks.find((l) => l.id === m.id) ?? null,
        preds: [],
        succs: [],
        el: void 0,
        // set below in constructor
        size: { x: 0, y: 0 },
        layer: -1,
        layoutNode: void 0,
        // set in makeLayoutNodes
        isLoopHeader: false,
        isBackedge: false,
        isRoot: false,
        isOSRTarget: false,
        isCatchEntrypoint: false
      };
      assert(block.ptr, "blocks must always have non-null ptrs");
      return block;
    });
    this.blocksByID = /* @__PURE__ */ new Map();
    this.blocksByPtr = /* @__PURE__ */ new Map();
    this.insPtrsByID = /* @__PURE__ */ new Map();
    this.insIDsByPtr = /* @__PURE__ */ new Map();
    for (const block of this.blocks) {
      this.blocksByID.set(block.id, block);
      this.blocksByPtr.set(block.ptr, block);
      for (const ins of block.mir.instructions) {
        this.insPtrsByID.set(ins.id, ins.ptr);
        this.insIDsByPtr.set(ins.ptr, ins.id);
      }
      if (block.lir) {
        for (const ins of block.lir.instructions) {
          this.insPtrsByID.set(ins.id, ins.ptr);
          this.insIDsByPtr.set(ins.ptr, ins.id);
        }
      }
    }
    for (const block of this.blocks) {
      block.preds = block.mir.predecessors.map((id) => must(this.blocksByID.get(id)));
      block.succs = block.mir.successors.map((id) => must(this.blocksByID.get(id)));
      block.isRoot = block.preds.length === 0;
      block.isOSRTarget = block.mir.attributes.includes("osr");
      block.isCatchEntrypoint = block.mir.attributes.includes("catch");
    }
    this.assignLayers();
    for (const block of this.blocks) {
      block.el = this.renderBlock(block);
    }
    for (const block of this.blocks) {
      block.size = {
        x: block.el.offsetWidth,
        y: block.el.offsetHeight
      };
    }
    this.runLayout();
    this.stabilizationTimeout = window.setTimeout(() => {
      this.stabilizationTimeout = null;
      let changed = false;
      for (const block of this.blocks) {
        const w = block.el.offsetWidth;
        const h = block.el.offsetHeight;
        if (Math.abs(block.size.x - w) > 1 || Math.abs(block.size.y - h) > 1) {
          block.size = { x: w, y: h };
          changed = true;
        }
      }
      if (changed) {
        this.runLayout();
      }
      this.layoutFinalized = true;
      log.log("Graph layout finalized.");
    }, 100);
    this.addEventListeners();
  }
  runLayout() {
    const [nodesByLayer, layerHeights, trackHeights] = this.layout();
    const children = Array.from(this.graphContainer.children);
    for (const child of children) {
      if (!child.classList.contains("ig-block")) {
        child.remove();
      }
    }
    this.render(nodesByLayer, layerHeights, trackHeights);
    if (this.deferredJumpToBlock) {
      const { blockPtr, opts } = this.deferredJumpToBlock;
      this.deferredJumpToBlock = null;
      this.jumpToBlock(blockPtr, opts);
    }
  }
  layout() {
    const layoutNodesByLayer = this.makeLayoutNodes();
    this.straightenEdges(layoutNodesByLayer);
    const trackHeights = this.finagleJoints(layoutNodesByLayer);
    const layerHeights = this.verticalize(layoutNodesByLayer, trackHeights);
    return [layoutNodesByLayer, layerHeights, trackHeights];
  }
  findLayoutRoots() {
    const newRoots = [];
    const osrBlocks = [];
    const roots = this.blocks.filter((b) => b.preds.length === 0);
    for (const root of roots) {
      newRoots.push(root);
      if (root.mir.attributes.includes("osr")) {
        osrBlocks.push(root);
      }
    }
    if (newRoots.length === 0 && this.blocks.length > 0) {
      const sorted = [...this.blocks].sort((a, b) => a.id - b.id);
      newRoots.push(sorted[0]);
    }
    return [newRoots, osrBlocks];
  }
  detectCycles() {
    const visited = /* @__PURE__ */ new Map();
    const isBackedge = /* @__PURE__ */ new Map();
    const detect = (u) => {
      visited.set(u, 1);
      for (const v of u.succs) {
        const status = visited.get(v) ?? 0;
        if (status === 1) {
          let s = isBackedge.get(u);
          if (!s) {
            s = /* @__PURE__ */ new Set();
            isBackedge.set(u, s);
          }
          s.add(v);
          u.isBackedge = true;
          v.isLoopHeader = true;
        } else if (status === 0) {
          detect(v);
        }
      }
      visited.set(u, 2);
    };
    for (const b of this.blocks) {
      if (!visited.has(b)) detect(b);
    }
    return isBackedge;
  }
  assignLayers() {
    const isBackedge = this.detectCycles();
    for (const b of this.blocks) b.layer = -1;
    const computeLayer = (u) => {
      if (u.layer !== -1) return u.layer;
      let maxParentLayer = -1;
      for (const p of u.preds) {
        const pBackedges = isBackedge.get(p);
        if (pBackedges && pBackedges.has(u)) continue;
        const pl = computeLayer(p);
        if (pl > maxParentLayer) maxParentLayer = pl;
      }
      u.layer = maxParentLayer + 1;
      return u.layer;
    };
    let maxLayer = 0;
    for (const b of this.blocks) {
      const l = computeLayer(b);
      if (l > maxLayer) maxLayer = l;
    }
    this.numLayers = maxLayer + 1;
    log.log(`Assigned generic layers. Max layer: ${maxLayer}`);
  }
  makeLayoutNodes() {
    log.group("makeLayoutNodes");
    function connectNodes(from, fromPort, to) {
      from.dstNodes[fromPort] = to;
      if (!to.srcNodes.includes(from)) {
        to.srcNodes.push(from);
      }
    }
    let blocksByLayer;
    {
      const blocksByLayerObj = {};
      for (const block of this.blocks) {
        const l = block.layer < 0 ? 0 : block.layer;
        if (!blocksByLayerObj[l]) {
          blocksByLayerObj[l] = [];
        }
        blocksByLayerObj[l].push(block);
      }
      blocksByLayer = Object.entries(blocksByLayerObj).map(([layer, blocks]) => [Number(layer), blocks]).sort((a, b) => a[0] - b[0]).map(([_, blocks]) => blocks);
    }
    const denseBlocksByLayer = [];
    for (let i = 0; i < this.numLayers; i++) {
      denseBlocksByLayer[i] = blocksByLayer.find((b) => b[0]?.layer === i) || [];
    }
    let nodeID = 0;
    const layoutNodesByLayer = denseBlocksByLayer.map(() => []);
    const activeForwardEdges = [];
    for (const [layer, blocks] of denseBlocksByLayer.entries()) {
      const terminatingEdges = [];
      for (const block of blocks) {
        for (let i = activeForwardEdges.length - 1; i >= 0; i--) {
          const edge = activeForwardEdges[i];
          if (edge.dstBlock === block) {
            terminatingEdges.unshift(edge);
            activeForwardEdges.splice(i, 1);
          }
        }
      }
      const dummiesByDest = /* @__PURE__ */ new Map();
      for (const edge of activeForwardEdges) {
        let dummy = dummiesByDest.get(edge.dstBlock.id);
        if (dummy) {
          connectNodes(edge.src, edge.srcPort, dummy);
        } else {
          dummy = {
            id: nodeID++,
            pos: { x: CONTENT_PADDING, y: CONTENT_PADDING },
            size: { x: 0, y: 0 },
            block: null,
            srcNodes: [],
            dstNodes: [],
            dstBlock: edge.dstBlock,
            jointOffsets: [],
            flags: 0
          };
          connectNodes(edge.src, edge.srcPort, dummy);
          layoutNodesByLayer[layer].push(dummy);
          dummiesByDest.set(edge.dstBlock.id, dummy);
        }
        edge.src = dummy;
        edge.srcPort = 0;
      }
      for (const block of blocks) {
        const node = {
          id: nodeID++,
          pos: { x: CONTENT_PADDING, y: CONTENT_PADDING },
          size: block.size,
          block,
          srcNodes: [],
          dstNodes: [],
          jointOffsets: [],
          flags: 0
        };
        block.layoutNode = node;
        layoutNodesByLayer[layer].push(node);
        for (const edge of terminatingEdges) {
          if (edge.dstBlock === block) {
            connectNodes(edge.src, edge.srcPort, node);
          }
        }
      }
      for (const block of blocks) {
        block.succs.forEach((succ, portIndex) => {
          if (succ.layer > layer) {
            if (succ.layer === layer + 1) {
              activeForwardEdges.push({ src: block.layoutNode, srcPort: portIndex, dstBlock: succ });
            } else {
              activeForwardEdges.push({ src: block.layoutNode, srcPort: portIndex, dstBlock: succ });
            }
          } else {
            let currentLayer = layer;
            let targetLayer = succ.layer;
            let prevNode = block.layoutNode;
            let prevPort = portIndex;
            for (let l = currentLayer; l >= targetLayer; l--) {
              const dummy = {
                id: nodeID++,
                pos: { x: CONTENT_PADDING, y: CONTENT_PADDING },
                size: { x: 0, y: 0 },
                block: null,
                srcNodes: [],
                dstNodes: [],
                dstBlock: succ,
                jointOffsets: [],
                flags: IMMINENT_BACKEDGE_DUMMY
              };
              layoutNodesByLayer[l].push(dummy);
              connectNodes(prevNode, prevPort, dummy);
              prevNode = dummy;
              prevPort = 0;
            }
            if (succ.layoutNode) {
              connectNodes(prevNode, prevPort, succ.layoutNode);
            }
          }
        });
      }
    }
    for (const nodes of layoutNodesByLayer) {
      for (let i = 0; i < nodes.length; i++) {
        if (nodes[i].block === null) {
          nodes[i].flags |= LEFTMOST_DUMMY;
        } else {
          break;
        }
      }
      for (let i = nodes.length - 1; i >= 0; i--) {
        if (nodes[i].block === null) {
          nodes[i].flags |= RIGHTMOST_DUMMY;
        } else {
          break;
        }
      }
    }
    log.groupEnd();
    return layoutNodesByLayer;
  }
  straightenEdges(layoutNodesByLayer) {
    const pushNeighbors = (nodes) => {
      for (let i = 0; i < nodes.length - 1; i++) {
        const node = nodes[i];
        const neighbor = nodes[i + 1];
        const firstNonDummy = node.block === null && neighbor.block !== null;
        const nodeRightPlusPadding = node.pos.x + node.size.x + (firstNonDummy ? PORT_START : 0) + BLOCK_GAP;
        neighbor.pos.x = Math.max(neighbor.pos.x, nodeRightPlusPadding);
      }
    };
    const straightenDummyRuns = () => {
      const dummyLinePositions = /* @__PURE__ */ new Map();
      for (const dummy of dummies(layoutNodesByLayer)) {
        const dst = dummy.dstBlock;
        let desiredX = dummy.pos.x;
        dummyLinePositions.set(dst, Math.max(dummyLinePositions.get(dst) ?? 0, desiredX));
      }
      for (const dummy of dummies(layoutNodesByLayer)) {
        const backedge = dummy.dstBlock;
        const x = dummyLinePositions.get(backedge);
        if (x) dummy.pos.x = x;
      }
      for (const nodes of layoutNodesByLayer) {
        pushNeighbors(nodes);
      }
    };
    const suckInLeftmostDummies = () => {
      const dummyRunPositions = /* @__PURE__ */ new Map();
      for (const nodes of layoutNodesByLayer) {
        let i = 0;
        let nextX = 0;
        for (; i < nodes.length; i++) {
          if (!(nodes[i].flags & LEFTMOST_DUMMY)) {
            nextX = nodes[i].pos.x;
            break;
          }
        }
        i -= 1;
        nextX -= BLOCK_GAP + PORT_START;
        for (; i >= 0; i--) {
          const dummy = nodes[i];
          let maxSafeX = nextX;
          for (const src of dummy.srcNodes) {
            const srcX = src.pos.x + src.dstNodes.indexOf(dummy) * PORT_SPACING;
            if (srcX < maxSafeX) {
              maxSafeX = srcX;
            }
          }
          dummy.pos.x = maxSafeX;
          nextX = dummy.pos.x - BLOCK_GAP;
          dummyRunPositions.set(dummy.dstBlock, Math.min(dummyRunPositions.get(dummy.dstBlock) ?? Infinity, maxSafeX));
        }
      }
      for (const dummy of dummies(layoutNodesByLayer)) {
        if (!(dummy.flags & LEFTMOST_DUMMY)) continue;
        const x = dummyRunPositions.get(dummy.dstBlock);
        if (x) dummy.pos.x = x;
      }
    };
    const straightenChildren = () => {
      for (let layer = 0; layer < layoutNodesByLayer.length - 1; layer++) {
        const nodes = layoutNodesByLayer[layer];
        pushNeighbors(nodes);
        let lastShifted = -1;
        for (const node of nodes) {
          for (const [srcPort, dst] of node.dstNodes.entries()) {
            if (!layoutNodesByLayer[layer + 1].includes(dst)) continue;
            let dstIndexInNextLayer = layoutNodesByLayer[layer + 1].indexOf(dst);
            if (dstIndexInNextLayer > lastShifted && dst.srcNodes[0] === node) {
              const srcPortOffset = PORT_START + PORT_SPACING * srcPort;
              const dstPortOffset = PORT_START;
              let xBefore = dst.pos.x;
              dst.pos.x = Math.max(dst.pos.x, node.pos.x + srcPortOffset - dstPortOffset);
              if (dst.pos.x !== xBefore) {
                lastShifted = dstIndexInNextLayer;
              }
            }
          }
        }
      }
    };
    const straightenConservative = () => {
      for (const nodes of layoutNodesByLayer) {
        for (let i = nodes.length - 1; i >= 0; i--) {
          const node = nodes[i];
          if (!node.block || node.block.isLoopHeader) continue;
          let deltasToTry = [];
          for (const parent of node.srcNodes) {
            const srcPortOffset = PORT_START + parent.dstNodes.indexOf(node) * PORT_SPACING;
            const dstPortOffset = PORT_START;
            deltasToTry.push(parent.pos.x + srcPortOffset - (node.pos.x + dstPortOffset));
          }
          for (const [srcPort, dst] of node.dstNodes.entries()) {
            if (dst.block === null && dst.dstBlock.isLoopHeader) continue;
            const srcPortOffset = PORT_START + srcPort * PORT_SPACING;
            const dstPortOffset = PORT_START;
            deltasToTry.push(dst.pos.x + dstPortOffset - (node.pos.x + srcPortOffset));
          }
          if (deltasToTry.includes(0)) continue;
          deltasToTry = deltasToTry.filter((d) => d > 0).sort((a, b) => a - b);
          for (const delta of deltasToTry) {
            let overlapsAny = false;
            for (let j = i + 1; j < nodes.length; j++) {
              const other = nodes[j];
              if (other.flags & RIGHTMOST_DUMMY) continue;
              const a1 = node.pos.x + delta, a2 = node.pos.x + delta + node.size.x;
              const b1 = other.pos.x - BLOCK_GAP, b2 = other.pos.x + other.size.x + BLOCK_GAP;
              const overlaps = a2 >= b1 && a1 <= b2;
              if (overlaps) overlapsAny = true;
            }
            if (!overlapsAny) {
              node.pos.x += delta;
              break;
            }
          }
        }
        pushNeighbors(nodes);
      }
    };
    const straightenNearlyStraightEdgesUp = () => {
      for (let layer = layoutNodesByLayer.length - 1; layer >= 0; layer--) {
        const nodes = layoutNodesByLayer[layer];
        pushNeighbors(nodes);
        for (const node of nodes) {
          for (const src of node.srcNodes) {
            if (src.block !== null) continue;
            const wiggle = Math.abs(src.pos.x - node.pos.x);
            if (wiggle <= NEARLY_STRAIGHT) {
              src.pos.x = Math.max(src.pos.x, node.pos.x);
              node.pos.x = Math.max(src.pos.x, node.pos.x);
            }
          }
        }
      }
    };
    const straightenNearlyStraightEdgesDown = () => {
      for (let layer = 0; layer < layoutNodesByLayer.length; layer++) {
        const nodes = layoutNodesByLayer[layer];
        pushNeighbors(nodes);
        for (const node of nodes) {
          if (node.dstNodes.length === 0) continue;
          const dst = node.dstNodes[0];
          if (dst.block !== null) continue;
          const wiggle = Math.abs(dst.pos.x - node.pos.x);
          if (wiggle <= NEARLY_STRAIGHT) {
            dst.pos.x = Math.max(dst.pos.x, node.pos.x);
            node.pos.x = Math.max(dst.pos.x, node.pos.x);
          }
        }
      }
    };
    function repeat(a, n) {
      const result = [];
      for (let i = 0; i < n; i++) {
        for (const item of a) result.push(item);
      }
      return result;
    }
    const passes = [
      ...repeat([
        straightenChildren,
        straightenDummyRuns
      ], LAYOUT_ITERATIONS),
      straightenDummyRuns,
      ...repeat([
        straightenNearlyStraightEdgesUp,
        straightenNearlyStraightEdgesDown
      ], NEARLY_STRAIGHT_ITERATIONS),
      straightenConservative,
      straightenDummyRuns,
      suckInLeftmostDummies
    ];
    for (const [i, pass] of passes.entries()) {
      if (i < STOP_AT_PASS) {
        pass();
      }
    }
  }
  finagleJoints(layoutNodesByLayer) {
    const trackHeights = [];
    for (const nodes of layoutNodesByLayer) {
      const joints = [];
      for (const node of nodes) {
        node.jointOffsets = new Array(node.dstNodes.length).fill(0);
        for (const [srcPort, dst] of node.dstNodes.entries()) {
          if (dst.pos.y < node.pos.y) continue;
          const x1 = node.pos.x + PORT_START + PORT_SPACING * srcPort;
          const x2 = dst.pos.x + PORT_START;
          if (Math.abs(x2 - x1) < 2 * ARROW_RADIUS) continue;
          joints.push({ x1, x2, src: node, srcPort, dst });
        }
      }
      joints.sort((a, b) => a.x1 - b.x1);
      const rightwardTracks = [];
      const leftwardTracks = [];
      nextJoint:
        for (const joint of joints) {
          const trackSet = joint.x2 - joint.x1 >= 0 ? rightwardTracks : leftwardTracks;
          let lastValidTrack = null;
          for (let i = trackSet.length - 1; i >= 0; i--) {
            const track = trackSet[i];
            let overlapsWithAnyInThisTrack = false;
            for (const otherJoint of track) {
              if (joint.dst === otherJoint.dst) {
                track.push(joint);
                continue nextJoint;
              }
              const al = Math.min(joint.x1, joint.x2), ar = Math.max(joint.x1, joint.x2);
              const bl = Math.min(otherJoint.x1, otherJoint.x2), br = Math.max(otherJoint.x1, otherJoint.x2);
              const overlaps = ar >= bl && al <= br;
              if (overlaps) {
                overlapsWithAnyInThisTrack = true;
                break;
              }
            }
            if (overlapsWithAnyInThisTrack) {
              break;
            } else {
              lastValidTrack = track;
            }
          }
          if (lastValidTrack) {
            lastValidTrack.push(joint);
          } else {
            trackSet.push([joint]);
          }
        }
      const tracksHeight = Math.max(0, rightwardTracks.length + leftwardTracks.length - 1) * JOINT_SPACING;
      let trackOffset = -tracksHeight / 2;
      for (const track of [...rightwardTracks.reverse(), ...leftwardTracks]) {
        for (const joint of track) {
          joint.src.jointOffsets[joint.srcPort] = trackOffset;
        }
        trackOffset += JOINT_SPACING;
      }
      trackHeights.push(tracksHeight);
    }
    return trackHeights;
  }
  verticalize(layoutNodesByLayer, trackHeights) {
    const layerHeights = new Array(layoutNodesByLayer.length);
    let nextLayerY = CONTENT_PADDING;
    for (let i = 0; i < layoutNodesByLayer.length; i++) {
      const nodes = layoutNodesByLayer[i];
      let layerHeight = 0;
      for (const node of nodes) {
        node.pos.y = nextLayerY;
        layerHeight = Math.max(layerHeight, node.size.y);
      }
      layerHeights[i] = layerHeight;
      nextLayerY += layerHeight + TRACK_PADDING + trackHeights[i] + TRACK_PADDING;
    }
    return layerHeights;
  }
  renderBlock(block) {
    const el = document.createElement("div");
    this.graphContainer.appendChild(el);
    el.classList.add("ig-block", "ig-bg-white");
    for (const att of block.mir.attributes) {
      el.classList.add(`ig-block-att-${att}`);
    }
    el.setAttribute("data-ig-block-ptr", `${block.ptr}`);
    el.setAttribute("data-ig-block-id", `${block.id}`);
    let desc = [];
    if (block.isRoot) {
      desc.push("(root)");
      el.classList.add(`ig-block-att-root`);
    }
    if (block.isCatchEntrypoint) {
      desc.push("(catch)");
    }
    if (block.isOSRTarget) {
      desc.push("(osr)");
    }
    if (block.isLoopHeader) {
      desc.push("(loop header)");
      el.classList.add(`ig-block-att-loopheader`);
    }
    if (block.isBackedge) {
      desc.push("(backedge)");
      el.classList.add(`ig-block-att-backedge`);
    }
    const header = document.createElement("div");
    header.classList.add("ig-block-header");
    header.innerText = `Block ${block.id}${desc.length === 0 ? "" : " " + desc.join(" ")}`;
    el.appendChild(header);
    const insnsContainer = document.createElement("div");
    insnsContainer.classList.add("ig-instructions");
    el.appendChild(insnsContainer);
    const insns = document.createElement("table");
    if (block.lir) {
      insns.innerHTML = `
        <colgroup>
          <col style="width: 1px">
          <col style="width: auto">
          ${this.sampleCounts ? `
            <col style="width: 1px">
            <col style="width: 1px">
          ` : ""}
        </colgroup>
        ${this.sampleCounts ? `
          <thead>
            <tr>
              <th></th>
              <th></th>
              <th class="ig-f6">Total</th>
              <th class="ig-f6">Self</th>
            </tr>
          </thead>
        ` : ""}
      `;
      for (const ins of block.lir.instructions) {
        insns.appendChild(this.renderLIRInstruction(ins));
      }
    } else {
      insns.innerHTML = `
        <colgroup>
          <col style="width: 1px">
          <col style="width: auto">
          <col style="width: 1px">
        </colgroup>
      `;
      for (const ins of block.mir.instructions) {
        insns.appendChild(this.renderMIRInstruction(ins));
      }
    }
    insnsContainer.appendChild(insns);
    for (const [i, succ] of block.succs.entries()) {
      const edgeLabel = document.createElement("div");
      edgeLabel.innerText = `#${succ.id}`;
      edgeLabel.classList.add("ig-edge-label");
      edgeLabel.style.left = `${PORT_START + PORT_SPACING * i}px`;
      el.appendChild(edgeLabel);
    }
    header.addEventListener("pointerdown", (e) => {
      e.preventDefault();
      e.stopPropagation();
    });
    header.addEventListener("click", (e) => {
      e.stopPropagation();
      if (!e.shiftKey) {
        this.selectedBlockPtrs.clear();
      }
      this.setSelection([], block.ptr);
    });
    return el;
  }
  render(nodesByLayer, layerHeights, trackHeights) {
    for (const nodes of nodesByLayer) {
      for (const node of nodes) {
        if (node.block !== null) {
          const block = node.block;
          block.el.style.left = `${node.pos.x}px`;
          block.el.style.top = `${node.pos.y}px`;
        }
      }
    }
    let maxX = 0, maxY = 0;
    for (const nodes of nodesByLayer) {
      for (const node of nodes) {
        maxX = Math.max(maxX, node.pos.x + node.size.x + CONTENT_PADDING);
        maxY = Math.max(maxY, node.pos.y + node.size.y + CONTENT_PADDING);
      }
    }
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    this.graphContainer.appendChild(svg);
    const trackPositions = (xs, ys) => {
      for (const x of xs) maxX = Math.max(maxX, x + CONTENT_PADDING);
      for (const y of ys) maxY = Math.max(maxY, y + CONTENT_PADDING);
    };
    for (let layer = 0; layer < nodesByLayer.length; layer++) {
      const nodes = nodesByLayer[layer];
      for (const node of nodes) {
        assert(node.dstNodes.length === node.jointOffsets.length, "must have a joint offset for each destination");
        for (const [i, dst] of node.dstNodes.entries()) {
          const x1 = node.pos.x + PORT_START + PORT_SPACING * i;
          let y1 = node.pos.y + node.size.y;
          const x2 = dst.pos.x + PORT_START;
          let y2 = dst.pos.y;
          const isUpward = y2 < y1;
          const isBackedgeDummyChain = (n) => (n.flags & IMMINENT_BACKEDGE_DUMMY) !== 0;
          if (node.block && dst.block === null && isUpward) {
            const ym = y1 + TRACK_PADDING;
            const arrow = arrowFromBlockToBackedgeDummy(x1, y1, x2, y2, ym);
            svg.appendChild(arrow);
            trackPositions([x1, x2], [y1, y2, ym]);
          } else if (node.block === null && dst.block !== null && Math.abs(y1 - y2) < 1) {
            const yHigh = y2 - BACKEDGE_GAP;
            const arrow = arrowBackedgeEnd(x1, y1, x2, y2, yHigh);
            svg.appendChild(arrow);
            trackPositions([x1, x2], [y1, y2, yHigh]);
          } else if (isUpward) {
            const ym = y1 - TRACK_PADDING;
            const arrow = upwardArrow(x1, y1, x2, y2, ym, dst.block !== null);
            svg.appendChild(arrow);
            trackPositions([x1, x2], [y1, y2, ym]);
          } else {
            const ym = y1 - node.size.y + layerHeights[layer] + TRACK_PADDING + trackHeights[layer] / 2 + node.jointOffsets[i];
            const arrow = downwardArrow(x1, y1, x2, y2, ym, dst.block !== null);
            svg.appendChild(arrow);
            trackPositions([x1, x2], [y1, y2, ym]);
          }
        }
      }
    }
    svg.setAttribute("width", `${maxX}`);
    svg.setAttribute("height", `${maxY}`);
    this.size = { x: maxX, y: maxY };
    if (+DEBUG) {
      for (const nodes of nodesByLayer) {
        for (const node of nodes) {
          const el = document.createElement("div");
          el.appendChild(document.createTextNode(String(node.id)));
          el.appendChild(document.createElement("br"));
          el.appendChild(document.createTextNode(`L:${node.block?.layer}`));
          el.appendChild(document.createElement("br"));
          el.appendChild(document.createTextNode(`<- ${node.srcNodes.map((n) => n.id)}`));
          el.appendChild(document.createElement("br"));
          el.appendChild(document.createTextNode(`-> ${node.dstNodes.map((n) => n.id)}`));
          el.style.position = "absolute";
          el.style.border = "1px solid black";
          el.style.backgroundColor = "white";
          el.style.left = `${node.pos.x}px`;
          el.style.top = `${node.pos.y}px`;
          el.style.whiteSpace = "nowrap";
          this.graphContainer.appendChild(el);
        }
      }
    }
    this.updateHighlightedInstructions();
    this.updateHotness();
  }
  renderMIRInstruction(ins) {
    const prettyOpcode = ins.opcode.replace("->", "\u2192").replace("<-", "\u2190");
    const row = document.createElement("tr");
    row.classList.add(
      "ig-ins",
      "ig-ins-mir",
      "ig-can-flash",
      ...ins.attributes.map((att) => `ig-ins-att-${att}`)
    );
    row.setAttribute("data-ig-ins-ptr", `${ins.ptr}`);
    row.setAttribute("data-ig-ins-id", `${ins.id}`);
    const num = document.createElement("td");
    num.classList.add("ig-ins-num");
    num.innerText = String(ins.id);
    row.appendChild(num);
    const opcode = document.createElement("td");
    const usePattern = /([A-Za-z0-9_<>]+)#(\d+)/g;
    let lastIndex = 0;
    let match;
    while ((match = usePattern.exec(prettyOpcode)) !== null) {
      if (match.index > lastIndex) {
        const textBefore = prettyOpcode.substring(lastIndex, match.index);
        opcode.appendChild(document.createTextNode(textBefore));
      }
      const useSpan = document.createElement("span");
      useSpan.className = "ig-use ig-highlightable";
      useSpan.setAttribute("data-ig-use", encodeURIComponent(match[2]));
      useSpan.textContent = `${match[1]}#${match[2]}`;
      opcode.appendChild(useSpan);
      lastIndex = usePattern.lastIndex;
    }
    if (lastIndex < prettyOpcode.length) {
      const textAfter = prettyOpcode.substring(lastIndex);
      opcode.appendChild(document.createTextNode(textAfter));
    }
    row.appendChild(opcode);
    const type = document.createElement("td");
    type.classList.add("ig-ins-type");
    type.innerText = ins.type === "None" ? "" : ins.type;
    row.appendChild(type);
    num.addEventListener("pointerdown", (e) => {
      e.preventDefault();
      e.stopPropagation();
    });
    num.addEventListener("click", () => {
      this.toggleInstructionHighlight(ins.ptr);
    });
    opcode.querySelectorAll(".ig-use").forEach((use) => {
      use.addEventListener("pointerdown", (e) => {
        e.preventDefault();
        e.stopPropagation();
      });
      use.addEventListener("click", (e) => {
        const id = parseInt(must(use.getAttribute("data-ig-use")), 10);
        this.jumpToInstruction(id, { zoom: 1 });
      });
    });
    return row;
  }
  renderLIRInstruction(ins) {
    const prettyOpcode = ins.opcode.replace("->", "\u2192").replace("<-", "\u2190");
    const row = document.createElement("tr");
    row.classList.add("ig-ins", "ig-ins-lir", "ig-hotness");
    row.setAttribute("data-ig-ins-ptr", `${ins.ptr}`);
    row.setAttribute("data-ig-ins-id", `${ins.id}`);
    const num = document.createElement("td");
    num.classList.add("ig-ins-num");
    num.innerText = String(ins.id);
    row.appendChild(num);
    const opcode = document.createElement("td");
    opcode.innerText = prettyOpcode;
    row.appendChild(opcode);
    if (this.sampleCounts) {
      const totalSampleCount = this.sampleCounts?.totalLineHits.get(ins.id) ?? 0;
      const selfSampleCount = this.sampleCounts?.selfLineHits.get(ins.id) ?? 0;
      const totalSamples = document.createElement("td");
      totalSamples.classList.add("ig-ins-samples");
      totalSamples.classList.toggle("ig-text-dim", totalSampleCount === 0);
      totalSamples.innerText = `${totalSampleCount}`;
      totalSamples.title = "Color by total count";
      row.appendChild(totalSamples);
      const selfSamples = document.createElement("td");
      selfSamples.classList.add("ig-ins-samples");
      selfSamples.classList.toggle("ig-text-dim", selfSampleCount === 0);
      selfSamples.innerText = `${selfSampleCount}`;
      selfSamples.title = "Color by self count";
      row.appendChild(selfSamples);
      for (const [i, el] of [totalSamples, selfSamples].entries()) {
        el.addEventListener("pointerdown", (e) => {
          e.preventDefault();
          e.stopPropagation();
        });
        el.addEventListener("click", () => {
          assert(i === SC_TOTAL || i === SC_SELF);
          this.heatmapMode = i;
          this.updateHotness();
        });
      }
    }
    num.addEventListener("pointerdown", (e) => {
      e.preventDefault();
      e.stopPropagation();
    });
    num.addEventListener("click", () => {
      this.toggleInstructionHighlight(ins.ptr);
    });
    return row;
  }
  renderSelection() {
    this.graphContainer.querySelectorAll(".ig-block").forEach((blockEl) => {
      const ptr = parseInt(must(blockEl.getAttribute("data-ig-block-ptr")), 10);
      blockEl.classList.toggle("ig-selected", this.selectedBlockPtrs.has(ptr));
      blockEl.classList.toggle("ig-last-selected", this.lastSelectedBlockPtr === ptr);
    });
  }
  removeNonexistentHighlights() {
    this.highlightedInstructions = this.highlightedInstructions.filter((hi) => {
      return this.graphContainer.querySelector(`.ig-ins[data-ig-ins-ptr="${hi.ptr}"]`);
    });
  }
  updateHighlightedInstructions() {
    for (const hi of this.highlightedInstructions) {
      assert(this.highlightedInstructions.filter((other) => other.ptr === hi.ptr).length === 1, `instruction ${hi.ptr} was highlighted more than once`);
    }
    this.graphContainer.querySelectorAll(".ig-ins, .ig-use").forEach((ins) => {
      clearHighlight(ins);
    });
    for (const hi of this.highlightedInstructions) {
      const color = this.instructionPalette[hi.paletteColor % this.instructionPalette.length];
      const row = this.graphContainer.querySelector(`.ig-ins[data-ig-ins-ptr="${hi.ptr}"]`);
      if (row) {
        highlight(row, color);
        const id = this.insIDsByPtr.get(hi.ptr);
        this.graphContainer.querySelectorAll(`.ig-use[data-ig-use="${id}"]`).forEach((use) => {
          highlight(use, color);
        });
      }
    }
  }
  updateHotness() {
    this.graphContainer.querySelectorAll(".ig-ins-lir").forEach((insEl) => {
      assert(insEl.classList.contains("ig-hotness"));
      const insID = parseInt(must(insEl.getAttribute("data-ig-ins-id")), 10);
      let hotness = 0;
      if (this.sampleCounts) {
        const counts = this.heatmapMode === SC_TOTAL ? this.sampleCounts.totalLineHits : this.sampleCounts.selfLineHits;
        hotness = (counts.get(insID) ?? 0) / this.maxSampleCounts[this.heatmapMode];
      }
      insEl.style.setProperty("--ig-hotness", `${hotness}`);
    });
  }
  addEventListeners() {
    this.viewport.addEventListener("wheel", (e) => {
      e.preventDefault();
      let newZoom = this.zoom;
      if (e.ctrlKey) {
        newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.zoom * Math.pow(ZOOM_SENSITIVITY, -e.deltaY * WHEEL_DELTA_SCALE)));
        const zoomDelta = newZoom / this.zoom - 1;
        this.zoom = newZoom;
        const { x: gx, y: gy } = this.viewport.getBoundingClientRect();
        const mouseOffsetX = e.clientX - gx - this.translation.x;
        const mouseOffsetY = e.clientY - gy - this.translation.y;
        this.translation.x -= mouseOffsetX * zoomDelta;
        this.translation.y -= mouseOffsetY * zoomDelta;
      } else {
        this.translation.x -= e.deltaX;
        this.translation.y -= e.deltaY;
      }
      const clampedT = this.clampTranslation(this.translation, newZoom);
      this.translation.x = clampedT.x;
      this.translation.y = clampedT.y;
      this.animating = false;
      this.updatePanAndZoom();
    });
    this.viewport.addEventListener("pointerdown", (e) => {
      if (e.pointerType === "mouse" && !(e.button === 0 || e.button === 1)) {
        return;
      }
      e.preventDefault();
      this.viewport.setPointerCapture(e.pointerId);
      this.startMousePos = {
        x: e.clientX,
        y: e.clientY
      };
      this.lastMousePos = {
        x: e.clientX,
        y: e.clientY
      };
      this.animating = false;
    });
    this.viewport.addEventListener("pointermove", (e) => {
      if (!this.viewport.hasPointerCapture(e.pointerId)) {
        return;
      }
      const dx = e.clientX - this.lastMousePos.x;
      const dy = e.clientY - this.lastMousePos.y;
      this.translation.x += dx;
      this.translation.y += dy;
      this.lastMousePos = {
        x: e.clientX,
        y: e.clientY
      };
      const clampedT = this.clampTranslation(this.translation, this.zoom);
      this.translation.x = clampedT.x;
      this.translation.y = clampedT.y;
      this.animating = false;
      this.updatePanAndZoom();
    });
    this.viewport.addEventListener("pointerup", (e) => {
      this.viewport.releasePointerCapture(e.pointerId);
      const THRESHOLD = 2;
      const deltaX = this.startMousePos.x - e.clientX;
      const deltaY = this.startMousePos.y - e.clientY;
      if (Math.abs(deltaX) <= THRESHOLD && Math.abs(deltaY) <= THRESHOLD) {
        this.setSelection([]);
      }
      this.animating = false;
    });
    const ro = new ResizeObserver((entries) => {
      assert(entries.length === 1);
      const rect = entries[0].contentRect;
      this.viewportSize.x = rect.width;
      this.viewportSize.y = rect.height;
    });
    ro.observe(this.viewport);
  }
  setSelection(blockPtrs, lastSelectedPtr = 0) {
    this.setSelectionRaw(blockPtrs, lastSelectedPtr);
    if (!lastSelectedPtr) {
      this.nav = {
        visited: [],
        currentIndex: -1,
        siblings: []
      };
    } else {
      this.nav = {
        visited: [lastSelectedPtr],
        currentIndex: 0,
        siblings: [lastSelectedPtr]
      };
    }
  }
  setSelectionRaw(blockPtrs, lastSelectedPtr) {
    this.selectedBlockPtrs.clear();
    for (const blockPtr of [...blockPtrs, lastSelectedPtr]) {
      if (this.blocksByPtr.has(blockPtr)) {
        this.selectedBlockPtrs.add(blockPtr);
      }
    }
    this.lastSelectedBlockPtr = this.blocksByPtr.has(lastSelectedPtr) ? lastSelectedPtr : 0;
    this.renderSelection();
  }
  navigate(dir) {
    const selected = this.lastSelectedBlockPtr;
    if (dir === "down" || dir === "up") {
      if (!selected) {
        const blocks = [...this.blocks].sort((a, b) => a.id - b.id);
        const rootBlocks = blocks.filter((b) => b.preds.length === 0);
        const leafBlocks = blocks.filter((b) => b.succs.length === 0);
        const fauxSiblings = dir === "down" ? rootBlocks : leafBlocks;
        const firstBlock = fauxSiblings[0];
        assert(firstBlock);
        this.setSelectionRaw([], firstBlock.ptr);
        this.nav = {
          visited: [firstBlock.ptr],
          currentIndex: 0,
          siblings: fauxSiblings.map((b) => b.ptr)
        };
      } else {
        const currentBlock = must(this.blocksByPtr.get(selected));
        const nextSiblings = (dir === "down" ? currentBlock.succs : currentBlock.preds).map((next) => next.ptr);
        if (currentBlock.ptr !== this.nav.visited[this.nav.currentIndex]) {
          this.nav.visited = [currentBlock.ptr];
          this.nav.currentIndex = 0;
        }
        const nextIndex = this.nav.currentIndex + (dir === "down" ? 1 : -1);
        if (0 <= nextIndex && nextIndex < this.nav.visited.length) {
          this.nav.currentIndex = nextIndex;
          this.nav.siblings = nextSiblings;
        } else {
          const next = nextSiblings[0];
          if (next !== void 0) {
            if (dir === "down") {
              this.nav.visited.push(next);
              this.nav.currentIndex += 1;
              assert(this.nav.currentIndex === this.nav.visited.length - 1);
            } else {
              this.nav.visited.unshift(next);
              assert(this.nav.currentIndex === 0);
            }
            this.nav.siblings = nextSiblings;
          }
        }
        this.setSelectionRaw([], this.nav.visited[this.nav.currentIndex]);
      }
    } else {
      if (selected !== void 0) {
        const i = this.nav.siblings.indexOf(selected);
        assert(i >= 0, "currently selected node should be in siblings array");
        const nextI = i + (dir === "right" ? 1 : -1);
        if (0 <= nextI && nextI < this.nav.siblings.length) {
          this.setSelectionRaw([], this.nav.siblings[nextI]);
        }
      }
    }
    assert(this.nav.visited.length === 0 || this.nav.siblings.includes(this.nav.visited[this.nav.currentIndex]), "expected currently visited node to be in the siblings array");
    assert(this.lastSelectedBlockPtr === 0 || this.nav.siblings.includes(this.lastSelectedBlockPtr), "expected currently selected block to be in siblings array");
  }
  toggleInstructionHighlight(insPtr, force) {
    this.removeNonexistentHighlights();
    const indexOfExisting = this.highlightedInstructions.findIndex((hi) => hi.ptr === insPtr);
    let remove = indexOfExisting >= 0;
    if (force !== void 0) {
      remove = !force;
    }
    if (remove) {
      if (indexOfExisting >= 0) {
        this.highlightedInstructions.splice(indexOfExisting, 1);
      }
    } else {
      if (indexOfExisting < 0) {
        let nextPaletteColor = 0;
        while (true) {
          if (this.highlightedInstructions.find((hi) => hi.paletteColor === nextPaletteColor)) {
            nextPaletteColor += 1;
            continue;
          }
          break;
        }
        this.highlightedInstructions.push({
          ptr: insPtr,
          paletteColor: nextPaletteColor
        });
      }
    }
    this.updateHighlightedInstructions();
  }
  clampTranslation(t, scale) {
    const minX = TRANSLATION_CLAMP_AMOUNT - this.size.x * scale;
    const maxX = this.viewportSize.x - TRANSLATION_CLAMP_AMOUNT;
    const minY = TRANSLATION_CLAMP_AMOUNT - this.size.y * scale;
    const maxY = this.viewportSize.y - TRANSLATION_CLAMP_AMOUNT;
    const newX = clamp(t.x, minX, maxX);
    const newY = clamp(t.y, minY, maxY);
    return { x: newX, y: newY };
  }
  updatePanAndZoom() {
    const clampedT = this.clampTranslation(this.translation, this.zoom);
    this.graphContainer.style.transform = `translate(${clampedT.x}px, ${clampedT.y}px) scale(${this.zoom})`;
  }
  /**
   * Converts from graph space to viewport space.
   */
  graph2viewport(v, translation = this.translation, zoom = this.zoom) {
    return {
      x: v.x * zoom + translation.x,
      y: v.y * zoom + translation.y
    };
  }
  /**
   * Converts from viewport space to graph space.
   */
  viewport2graph(v, translation = this.translation, zoom = this.zoom) {
    return {
      x: (v.x - translation.x) / zoom,
      y: (v.y - translation.y) / zoom
    };
  }
  /**
   * Pans and zooms the graph such that the given x and y in graph space are in
   * the top left of the viewport.
   */
  async goToGraphCoordinates(coords, { zoom = this.zoom, animate = true }) {
    const newTranslation = { x: -coords.x * zoom, y: -coords.y * zoom };
    if (!animate) {
      this.animating = false;
      this.translation.x = newTranslation.x;
      this.translation.y = newTranslation.y;
      this.zoom = zoom;
      this.updatePanAndZoom();
      await new Promise((res) => setTimeout(res, 0));
      return;
    }
    this.targetTranslation = newTranslation;
    this.targetZoom = zoom;
    if (this.animating) {
      return;
    }
    this.animating = true;
    let lastTime = performance.now();
    while (this.animating) {
      const now = await new Promise((res) => requestAnimationFrame(res));
      const dt = (now - lastTime) / 1e3;
      lastTime = now;
      const THRESHOLD_T = 1, THRESHOLD_ZOOM = 0.01;
      const R = 1e-6;
      const dx = this.targetTranslation.x - this.translation.x;
      const dy = this.targetTranslation.y - this.translation.y;
      const dzoom = this.targetZoom - this.zoom;
      this.translation.x = filerp(this.translation.x, this.targetTranslation.x, R, dt);
      this.translation.y = filerp(this.translation.y, this.targetTranslation.y, R, dt);
      this.zoom = filerp(this.zoom, this.targetZoom, R, dt);
      this.updatePanAndZoom();
      if (Math.abs(dx) <= THRESHOLD_T && Math.abs(dy) <= THRESHOLD_T && Math.abs(dzoom) <= THRESHOLD_ZOOM) {
        this.translation.x = this.targetTranslation.x;
        this.translation.y = this.targetTranslation.y;
        this.zoom = this.targetZoom;
        this.animating = false;
        this.updatePanAndZoom();
        break;
      }
    }
    await new Promise((res) => setTimeout(res, 0));
  }
  jumpToBlock(blockPtr, { zoom = this.zoom, animate = true, viewportPos } = {}) {
    const block = this.blocksByPtr.get(blockPtr);
    if (!block) {
      return Promise.resolve();
    }
    if (!block.layoutNode) {
      this.deferredJumpToBlock = { blockPtr, opts: { zoom, animate, viewportPos } };
      return Promise.resolve();
    }
    let graphCoords;
    if (viewportPos) {
      graphCoords = {
        x: block.layoutNode.pos.x - viewportPos.x / zoom,
        y: block.layoutNode.pos.y - viewportPos.y / zoom
      };
    } else {
      graphCoords = this.graphPosToCenterRect(block.layoutNode.pos, block.layoutNode.size, zoom);
    }
    return this.goToGraphCoordinates(graphCoords, { zoom, animate });
  }
  async jumpToInstruction(insID, { zoom = this.zoom, animate = true }) {
    const insEl = this.graphContainer.querySelector(`.ig-ins[data-ig-ins-id="${insID}"]`);
    if (!insEl) {
      return;
    }
    const insRect = insEl.getBoundingClientRect();
    const graphRect = this.graphContainer.getBoundingClientRect();
    const x = (insRect.x - graphRect.x) / this.zoom;
    const y = (insRect.y - graphRect.y) / this.zoom;
    const width = insRect.width / this.zoom;
    const height = insRect.height / this.zoom;
    const coords = this.graphPosToCenterRect({ x, y }, { x: width, y: height }, zoom);
    insEl.classList.add("ig-flash");
    await this.goToGraphCoordinates(coords, { zoom, animate });
    insEl.classList.remove("ig-flash");
  }
  /**
   * Returns the position in graph space that, if panned to, will center the
   * given graph-space rectangle in the viewport.
   */
  graphPosToCenterRect(pos, size, zoom) {
    const viewportWidth = this.viewportSize.x / zoom;
    const viewportHeight = this.viewportSize.y / zoom;
    const xPadding = Math.max(20 / zoom, (viewportWidth - size.x) / 2);
    const yPadding = Math.max(20 / zoom, (viewportHeight - size.y) / 2);
    const x = pos.x - xPadding;
    const y = pos.y - yPadding;
    return { x, y };
  }
  exportState() {
    const state = {
      translation: this.translation,
      zoom: this.zoom,
      heatmapMode: this.heatmapMode,
      highlightedInstructions: this.highlightedInstructions,
      selectedBlockPtrs: this.selectedBlockPtrs,
      lastSelectedBlockPtr: this.lastSelectedBlockPtr,
      viewportPosOfSelectedBlock: void 0
    };
    if (this.lastSelectedBlockPtr) {
      state.viewportPosOfSelectedBlock = this.graph2viewport(must(this.blocksByPtr.get(this.lastSelectedBlockPtr)).layoutNode.pos);
    }
    return state;
  }
  restoreState(state, opts) {
    this.translation.x = state.translation.x;
    this.translation.y = state.translation.y;
    this.zoom = state.zoom;
    this.heatmapMode = state.heatmapMode;
    this.highlightedInstructions = state.highlightedInstructions;
    this.setSelection(Array.from(state.selectedBlockPtrs), state.lastSelectedBlockPtr);
    this.updatePanAndZoom();
    this.updateHotness();
    this.updateHighlightedInstructions();
    if (opts.preserveSelectedBlockPosition) {
      this.jumpToBlock(this.lastSelectedBlockPtr, {
        zoom: this.zoom,
        animate: false,
        viewportPos: state.viewportPosOfSelectedBlock
      });
    }
  }
};
function* dummies(layoutNodesByLayer) {
  for (const nodes of layoutNodesByLayer) {
    for (const node of nodes) {
      if (node.block === null) {
        yield node;
      }
    }
  }
}
function downwardArrow(x1, y1, x2, y2, ym, doArrowhead, stroke = 1) {
  const r = ARROW_RADIUS;
  if (stroke % 2 === 1) {
    x1 += 0.5;
    x2 += 0.5;
    ym += 0.5;
  }
  let path = "";
  path += `M ${x1} ${y1} `;
  if (Math.abs(x2 - x1) < 2 * r) {
    path += `C ${x1} ${y1 + (y2 - y1) / 3} ${x2} ${y1 + 2 * (y2 - y1) / 3} ${x2} ${y2} `;
  } else {
    const dir = Math.sign(x2 - x1);
    path += `L ${x1} ${ym - r} `;
    path += `A ${r} ${r} 0 0 ${dir > 0 ? 0 : 1} ${x1 + r * dir} ${ym} `;
    path += `L ${x2 - r * dir} ${ym} `;
    path += `A ${r} ${r} 0 0 ${dir > 0 ? 1 : 0} ${x2} ${ym + r} `;
    path += `L ${x2} ${y2} `;
  }
  const g = document.createElementNS("http://www.w3.org/2000/svg", "g");
  const p = document.createElementNS("http://www.w3.org/2000/svg", "path");
  p.setAttribute("d", path);
  p.setAttribute("fill", "none");
  p.setAttribute("stroke", "black");
  p.setAttribute("stroke-width", `${stroke} `);
  g.appendChild(p);
  if (doArrowhead) {
    const v = arrowhead(x2, y2, 180);
    g.appendChild(v);
  }
  return g;
}
function upwardArrow(x1, y1, x2, y2, ym, doArrowhead, stroke = 1) {
  const r = ARROW_RADIUS;
  if (stroke % 2 === 1) {
    x1 += 0.5;
    x2 += 0.5;
    ym += 0.5;
  }
  let path = "";
  path += `M ${x1} ${y1} `;
  if (Math.abs(x2 - x1) < 2 * r) {
    path += `C ${x1} ${y1 + (y2 - y1) / 3} ${x2} ${y1 + 2 * (y2 - y1) / 3} ${x2} ${y2} `;
  } else {
    const dir = Math.sign(x2 - x1);
    path += `L ${x1} ${ym + r} `;
    path += `A ${r} ${r} 0 0 ${dir > 0 ? 1 : 0} ${x1 + r * dir} ${ym} `;
    path += `L ${x2 - r * dir} ${ym} `;
    path += `A ${r} ${r} 0 0 ${dir > 0 ? 0 : 1} ${x2} ${ym - r} `;
    path += `L ${x2} ${y2} `;
  }
  const g = document.createElementNS("http://www.w3.org/2000/svg", "g");
  const p = document.createElementNS("http://www.w3.org/2000/svg", "path");
  p.setAttribute("d", path);
  p.setAttribute("fill", "none");
  p.setAttribute("stroke", "black");
  p.setAttribute("stroke-width", `${stroke} `);
  g.appendChild(p);
  if (doArrowhead) {
    const v = arrowhead(x2, y2, 0);
    g.appendChild(v);
  }
  return g;
}
function arrowFromBlockToBackedgeDummy(x1, y1, x2, y2, ym, stroke = 1) {
  const r = ARROW_RADIUS;
  if (stroke % 2 === 1) {
    x1 += 0.5;
    x2 += 0.5;
    ym += 0.5;
  }
  let path = "";
  path += `M ${x1} ${y1} `;
  path += `L ${x1} ${ym - r} `;
  path += `A ${r} ${r} 0 0 0 ${x1 + r} ${ym} `;
  path += `L ${x2 - r} ${ym} `;
  path += `A ${r} ${r} 0 0 0 ${x2} ${ym - r} `;
  path += `L ${x2} ${y2} `;
  const g = document.createElementNS("http://www.w3.org/2000/svg", "g");
  const p = document.createElementNS("http://www.w3.org/2000/svg", "path");
  p.setAttribute("d", path);
  p.setAttribute("fill", "none");
  p.setAttribute("stroke", "black");
  p.setAttribute("stroke-width", `${stroke} `);
  g.appendChild(p);
  return g;
}
function arrowBackedgeEnd(x1, y1, x2, y2, yHigh, stroke = 1) {
  const r = ARROW_RADIUS;
  if (stroke % 2 === 1) {
    x1 += 0.5;
    x2 += 0.5;
    y1 += 0.5;
    y2 += 0.5;
    yHigh += 0.5;
  }
  let path = "";
  path += `M ${x1} ${y1} `;
  path += `L ${x1} ${yHigh + r} `;
  path += `A ${r} ${r} 0 0 0 ${x1 - r} ${yHigh} `;
  path += `L ${x2 + r} ${yHigh} `;
  path += `A ${r} ${r} 0 0 0 ${x2} ${yHigh + r} `;
  path += `L ${x2} ${y2} `;
  const g = document.createElementNS("http://www.w3.org/2000/svg", "g");
  const p = document.createElementNS("http://www.w3.org/2000/svg", "path");
  p.setAttribute("d", path);
  p.setAttribute("fill", "none");
  p.setAttribute("stroke", "black");
  p.setAttribute("stroke-width", `${stroke} `);
  g.appendChild(p);
  const v = arrowhead(x2, y2, 180);
  g.appendChild(v);
  return g;
}
function arrowhead(x, y, rot, size = 5) {
  const p = document.createElementNS("http://www.w3.org/2000/svg", "path");
  p.setAttribute("d", `M 0 0 L ${-size} ${size * 1.5} L ${size} ${size * 1.5} Z`);
  p.setAttribute("transform", `translate(${x}, ${y}) rotate(${rot})`);
  return p;
}
function highlight(el, color) {
  el.classList.add("ig-highlight");
  el.style.setProperty("--ig-highlight-color", color);
}
function clearHighlight(el) {
  el.classList.remove("ig-highlight");
  el.style.setProperty("--ig-highlight-color", "transparent");
}

// src/GraphViewer.ts
var GraphViewer = class {
  constructor(root, {
    func,
    pass = 0,
    sampleCounts
  }) {
    this.graph = null;
    this.func = func;
    this.passNumber = pass;
    this.sampleCounts = sampleCounts;
    this.keyPasses = [null, null, null, null];
    {
      let lastPass = null;
      for (const [i, pass2] of func.passes.entries()) {
        if (pass2.mir.blocks.length > 0) {
          if (this.keyPasses[0] === null) {
            this.keyPasses[0] = i;
          }
          if (pass2.lir.blocks.length === 0) {
            this.keyPasses[1] = i;
          }
        }
        if (pass2.lir.blocks.length > 0) {
          if (lastPass?.lir.blocks.length === 0) {
            this.keyPasses[2] = i;
          }
          this.keyPasses[3] = i;
        }
        lastPass = pass2;
      }
    }
    this.redundantPasses = [];
    {
      let lastPass = null;
      for (const [i, pass2] of func.passes.entries()) {
        if (lastPass === null) {
          lastPass = pass2;
          continue;
        }
        if (dequal(lastPass.mir, pass2.mir) && dequal(lastPass.lir, pass2.lir)) {
          this.redundantPasses.push(i);
        }
        lastPass = pass2;
      }
    }
    this.viewport = E("div", ["ig-flex-grow-1", "ig-overflow-hidden"], (div) => {
      div.style.position = "relative";
    });
    this.sidebarLinks = func.passes.map((pass2, i) => E("a", ["ig-link-normal", "ig-pv1", "ig-ph2", "ig-flex", "ig-g2"], (a) => {
      a.href = "#";
      a.addEventListener("click", (e) => {
        e.preventDefault();
        this.switchPass(i);
      });
      a.style.minWidth = "100%";
    }, [
      E("div", ["ig-w1", "ig-tr", "ig-f6", "ig-text-dim"], (div) => {
        div.style.paddingTop = "0.08rem";
        div.style.flexShrink = "0";
      }, [`${i}`]),
      E("div", [this.redundantPasses.includes(i) && "ig-text-dim"], (div) => {
        div.style.whiteSpace = "nowrap";
      }, [pass2.name])
    ]));
    this.container = E("div", ["ig-absolute", "ig-absolute-fill", "ig-flex"], () => {
    }, [
      E("div", ["ig-w5", "ig-br", "ig-flex-shrink-0", "ig-overflow-y-auto", "ig-bg-white"], (div) => {
        div.style.overflowX = "auto";
      }, [
        ...this.sidebarLinks
      ]),
      this.viewport
    ]);
    root.appendChild(this.container);
    this.keydownHandler = this.keydownHandler.bind(this);
    this.tweakHandler = this.tweakHandler.bind(this);
    window.addEventListener("keydown", this.keydownHandler);
    window.addEventListener("tweak", this.tweakHandler);
    this.update();
  }
  destroy() {
    this.container.remove();
    window.removeEventListener("keydown", this.keydownHandler);
    window.removeEventListener("tweak", this.tweakHandler);
  }
  update() {
    for (const [i, link] of this.sidebarLinks.entries()) {
      link.classList.toggle("ig-bg-primary", this.passNumber === i);
    }
    const previousState = this.graph?.exportState();
    this.viewport.innerHTML = "";
    this.graph = null;
    const pass = this.func.passes[this.passNumber];
    if (pass) {
      try {
        this.graph = new Graph(this.viewport, pass, { sampleCounts: this.sampleCounts });
        if (previousState) {
          this.graph.restoreState(previousState, { preserveSelectedBlockPosition: true });
        }
      } catch (e) {
        this.viewport.innerHTML = "An error occurred while laying out the graph. See console.";
        console.error(e);
      }
    }
  }
  switchPass(pass) {
    this.passNumber = pass;
    this.update();
  }
  keydownHandler(e) {
    switch (e.key) {
      case "w":
      case "s":
        {
          this.graph?.navigate(e.key === "s" ? "down" : "up");
          this.graph?.jumpToBlock(this.graph.lastSelectedBlockPtr);
        }
        break;
      case "a":
      case "d":
        {
          this.graph?.navigate(e.key === "d" ? "right" : "left");
          this.graph?.jumpToBlock(this.graph.lastSelectedBlockPtr);
        }
        break;
      case "f":
        {
          for (let i = this.passNumber + 1; i < this.func.passes.length; i++) {
            if (!this.redundantPasses.includes(i)) {
              this.switchPass(i);
              break;
            }
          }
        }
        break;
      case "r":
        {
          for (let i = this.passNumber - 1; i >= 0; i--) {
            if (!this.redundantPasses.includes(i)) {
              this.switchPass(i);
              break;
            }
          }
        }
        break;
      case "1":
      case "2":
      case "3":
      case "4":
        {
          const keyPassIndex = ["1", "2", "3", "4"].indexOf(e.key);
          const keyPass = this.keyPasses[keyPassIndex];
          if (typeof keyPass === "number") {
            this.switchPass(keyPass);
          }
        }
        break;
      case "c":
        {
          const selected = this.graph?.blocksByPtr.get(this.graph?.lastSelectedBlockPtr ?? -1);
          if (selected) {
            this.graph?.jumpToBlock(selected.ptr, { zoom: 1 });
          }
        }
        break;
    }
    ;
  }
  tweakHandler() {
    this.update();
  }
};

// www/main.ts
var searchParams = new URL(window.location.toString()).searchParams;
var initialFuncIndex = searchParams.has("func") ? parseInt(searchParams.get("func"), 10) : void 0;
var initialPass = searchParams.has("pass") ? parseInt(searchParams.get("pass"), 10) : void 0;
var MenuBar = class {
  constructor(props) {
    this.exportButton = null;
    this.ionjson = null;
    this.funcIndex = initialFuncIndex ?? 0;
    this.funcSelected = props.funcSelected;
    this.funcSelector = E("div", [], () => {
    }, [
      "Function",
      E("input", ["ig-w3"], (input) => {
        input.type = "number";
        input.min = "1";
        input.addEventListener("input", () => {
          this.switchFunc(parseInt(input.value, 10) - 1);
        });
      }, []),
      " / ",
      E("span", ["num-functions"])
    ]);
    this.funcSelectorNone = E("div", [], () => {
    }, ["No functions to display."]);
    this.funcName = E("div");
    this.root = E("div", ["ig-bb", "ig-flex", "ig-bg-white"], () => {
    }, [
      E("div", ["ig-pv2", "ig-ph3", "ig-flex", "ig-g2", "ig-items-center", "ig-br", "ig-hide-if-empty"], () => {
      }, [
        props.browse && E("div", [], () => {
        }, [
          E("input", [], (input) => {
            input.type = "file";
            input.addEventListener("change", (e) => {
              const input2 = e.target;
              if (!input2.files?.length) {
                return;
              }
              this.fileSelected(input2.files[0]);
            });
          })
        ]),
        this.funcSelector,
        this.funcSelectorNone
      ]),
      E("div", ["ig-flex-grow-1", "ig-pv2", "ig-ph3", "ig-flex", "ig-g2", "ig-items-center"], () => {
      }, [
        this.funcName,
        E("div", ["ig-flex-grow-1"]),
        props.export && E("div", [], () => {
        }, [
          E("button", [], (button) => {
            this.exportButton = button;
            button.addEventListener("click", () => {
              this.exportStandalone();
            });
          }, ["Export"])
        ])
      ])
    ]);
    this.update();
  }
  async fileSelected(file) {
    const newJSON = JSON.parse(await file.text());
    this.ionjson = migrate(newJSON);
    this.switchFunc(0);
    this.update();
  }
  switchIonJSON(ionjson) {
    this.ionjson = ionjson;
    this.switchFunc(this.funcIndex);
  }
  switchFunc(funcIndex) {
    funcIndex = Math.max(0, Math.min(this.numFunctions() - 1, funcIndex));
    this.funcIndex = isNaN(funcIndex) ? 0 : funcIndex;
    this.funcSelected(this.ionjson?.functions[this.funcIndex] ?? null);
    this.update();
  }
  numFunctions() {
    return this.ionjson?.functions.length ?? 0;
  }
  update() {
    const funcIndexValid = 0 <= this.funcIndex && this.funcIndex < this.numFunctions();
    this.funcSelector.hidden = this.numFunctions() <= 1;
    this.funcSelectorNone.hidden = !(this.ionjson && this.numFunctions() === 0);
    const funcInput = this.funcSelector.querySelector("input");
    funcInput.max = `${this.numFunctions()}`;
    funcInput.value = `${this.funcIndex + 1}`;
    this.funcSelector.querySelector(".num-functions").innerHTML = `${this.numFunctions()}`;
    this.funcName.hidden = !funcIndexValid;
    this.funcName.innerText = `${this.ionjson?.functions[this.funcIndex].name ?? ""}`;
    if (this.exportButton) {
      this.exportButton.disabled = !this.ionjson || !funcIndexValid;
    }
  }
  async exportStandalone() {
    const ion = must(this.ionjson);
    const name = ion.functions[this.funcIndex].name;
    const result = { version: 1, functions: [ion.functions[this.funcIndex]] };
    const template = window.__standaloneTemplate ?? await (await fetch("./standalone.html")).text();
    const output = template.replace(/\{\{\s*IONJSON\s*\}\}/, JSON.stringify(result));
    const url = URL.createObjectURL(new Blob([output], { type: "text/html;charset=utf-8" }));
    const a = document.createElement("a");
    a.href = url;
    a.download = `iongraph-${name}.html`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  }
};
var WebUI = class {
  constructor() {
    this.menuBar = new MenuBar({
      browse: true,
      export: true,
      funcSelected: (f) => this.switchFunc(f)
    });
    this.func = null;
    this.sampleCountsFromFile = void 0;
    this.graph = null;
    this.loadStuffFromQueryParams();
    this.graphContainer = E("div", ["ig-relative", "ig-flex-basis-0", "ig-flex-grow-1", "ig-overflow-hidden"]);
    this.root = E("div", ["ig-absolute", "ig-absolute-fill", "ig-flex", "ig-flex-column"], (root) => {
      root.addEventListener("dragenter", (e) => {
        e.preventDefault();
        e.stopPropagation();
      });
      root.addEventListener("dragover", (e) => {
        e.preventDefault();
        e.stopPropagation();
      });
      root.addEventListener("drop", (e) => {
        e.preventDefault();
        e.stopPropagation();
        const files = e.dataTransfer?.files;
        if (files && files.length > 0) {
          this.menuBar.fileSelected(files[0]);
        }
      });
    }, [
      this.menuBar.root,
      this.graphContainer
    ]);
    this.update();
  }
  update() {
    if (this.graph) {
      this.graph.destroy();
    }
    if (this.func) {
      this.graph = new GraphViewer(this.graphContainer, {
        func: this.func,
        pass: initialPass,
        sampleCounts: this.sampleCountsFromFile
      });
    }
  }
  loadStuffFromQueryParams() {
    (async () => {
      const searchFile = searchParams.get("file");
      if (searchFile) {
        const res = await fetch(searchFile);
        const json = await res.json();
        const migrated = migrate(json);
        this.menuBar.switchIonJSON(migrated);
      }
    })();
    (async () => {
      const sampleCountsFile = searchParams.get("sampleCounts");
      if (sampleCountsFile) {
        const res = await fetch(sampleCountsFile);
        const json = await res.json();
        this.sampleCountsFromFile = {
          selfLineHits: new Map(json["selfLineHits"]),
          totalLineHits: new Map(json["totalLineHits"])
        };
        this.update();
      }
    })();
  }
  switchFunc(func) {
    this.func = func;
    this.update();
  }
};
var StandaloneUI = class {
  constructor() {
    this.menuBar = new MenuBar({
      funcSelected: (f) => this.switchFunc(f)
    });
    this.func = null;
    this.graph = null;
    this.graphContainer = E("div", ["ig-relative", "ig-flex-basis-0", "ig-flex-grow-1", "ig-overflow-hidden"]);
    this.root = E("div", ["ig-absolute", "ig-absolute-fill", "ig-flex", "ig-flex-column"], () => {
    }, [
      this.menuBar.root,
      this.graphContainer
    ]);
  }
  update() {
    if (this.graph) {
      this.graph.destroy();
    }
    if (this.func) {
      this.graph = new GraphViewer(this.graphContainer, {
        func: this.func,
        pass: initialPass
      });
    }
  }
  setIonJSON(ion) {
    this.menuBar.switchIonJSON(ion);
  }
  switchFunc(func) {
    this.func = func;
    this.update();
  }
};
export {
  StandaloneUI,
  WebUI
};
//# sourceMappingURL=main.js.map
