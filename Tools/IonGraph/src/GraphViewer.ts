import { dequal } from "./dequal.js";
import { E } from "./dom.js";
import { Graph } from "./Graph.js";
import type { BlockPtr, Func, Pass, SampleCounts } from "./iongraph.js";

type KeyPasses = [number | null, number | null, number | null, number | null];

export interface GraphViewerProps {
  func: Func,
  pass?: number,

  sampleCounts?: SampleCounts,
}

export class GraphViewer {
  func: Func;
  passNumber: number;
  keyPasses: KeyPasses;
  redundantPasses: number[];
  sampleCounts: SampleCounts | undefined;

  container: HTMLDivElement;
  viewport: HTMLDivElement;
  graph: Graph | null;
  sidebarLinks: HTMLAnchorElement[];

  constructor(root: HTMLElement, {
    func,
    pass = 0,

    sampleCounts
  }: GraphViewerProps) {
    this.graph = null;
    this.func = func;
    this.passNumber = pass;
    this.sampleCounts = sampleCounts;

    this.keyPasses = [null, null, null, null];
    {
      let lastPass: Pass | null = null;
      for (const [i, pass] of func.passes.entries()) {
        if (pass.mir.blocks.length > 0) {
          if (this.keyPasses[0] === null) {
            this.keyPasses[0] = i;
          }
          if (pass.lir.blocks.length === 0) {
            this.keyPasses[1] = i;
          }
        }
        if (pass.lir.blocks.length > 0) {
          if (lastPass?.lir.blocks.length === 0) {
            this.keyPasses[2] = i;
          }
          this.keyPasses[3] = i;
        }

        lastPass = pass;
      }
    }

    this.redundantPasses = [];
    {
      let lastPass: Pass | null = null;
      for (const [i, pass] of func.passes.entries()) {
        if (lastPass === null) {
          lastPass = pass;
          continue;
        }

        if (dequal(lastPass.mir, pass.mir) && dequal(lastPass.lir, pass.lir)) {
          this.redundantPasses.push(i);
        }

        lastPass = pass;
      }
    }

    this.viewport = E("div", ["ig-flex-grow-1", "ig-overflow-hidden"], div => {
      div.style.position = "relative";
    })
    this.sidebarLinks = func.passes.map((pass, i) => (
      E("a", ["ig-link-normal", "ig-pv1", "ig-ph2", "ig-flex", "ig-g2"], a => {
        a.href = "#";
        a.addEventListener("click", e => {
          e.preventDefault();
          this.switchPass(i);
        });
        a.style.minWidth = "100%";
      }, [
        E("div", ["ig-w1", "ig-tr", "ig-f6", "ig-text-dim"], div => {
          div.style.paddingTop = "0.08rem";
          div.style.flexShrink = "0";
        }, [`${i}`]),
        E("div", [this.redundantPasses.includes(i) && "ig-text-dim"], div => {
          div.style.whiteSpace = "nowrap";
        }, [pass.name]),
      ])
    ));
    this.container = E("div", ["ig-absolute", "ig-absolute-fill", "ig-flex"], () => { }, [
      E("div", ["ig-w5", "ig-br", "ig-flex-shrink-0", "ig-overflow-y-auto", "ig-bg-white"], div => {
        div.style.overflowX = "auto";
      }, [
        ...this.sidebarLinks,
      ]),
      this.viewport,
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
    // Update sidebar
    for (const [i, link] of this.sidebarLinks.entries()) {
      link.classList.toggle("ig-bg-primary", this.passNumber === i);
    }

    // Update graph
    const previousState = this.graph?.exportState();
    this.viewport.innerHTML = "";
    this.graph = null;
    const pass: Pass | undefined = this.func.passes[this.passNumber];
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

  switchPass(pass: number) {
    this.passNumber = pass;
    this.update();
  }

  keydownHandler(e: KeyboardEvent) {
    switch (e.key) {
      case "w":
      case "s": {
        this.graph?.navigate(e.key === "s" ? "down" : "up");
        this.graph?.jumpToBlock(this.graph.lastSelectedBlockPtr);
      } break;
      case "a":
      case "d": {
        this.graph?.navigate(e.key === "d" ? "right" : "left");
        this.graph?.jumpToBlock(this.graph.lastSelectedBlockPtr);
      } break;

      case "f": {
        for (let i = this.passNumber + 1; i < this.func.passes.length; i++) {
          if (!this.redundantPasses.includes(i)) {
            this.switchPass(i);
            break;
          }
        }
      } break;
      case "r": {
        for (let i = this.passNumber - 1; i >= 0; i--) {
          if (!this.redundantPasses.includes(i)) {
            this.switchPass(i);
            break;
          }
        }
      } break;
      case "1":
      case "2":
      case "3":
      case "4": {
        const keyPassIndex = ["1", "2", "3", "4"].indexOf(e.key);
        const keyPass = this.keyPasses[keyPassIndex];
        if (typeof keyPass === "number") {
          this.switchPass(keyPass);
        }
      } break;

      case "c": {
        const selected = this.graph?.blocksByPtr.get(this.graph?.lastSelectedBlockPtr ?? -1 as BlockPtr);
        if (selected) {
          this.graph?.jumpToBlock(selected.ptr, { zoom: 1 });
        }
      } break;
    };
  }

  tweakHandler() {
    this.update();
  }
}
