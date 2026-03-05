import { migrate, type IonJSON, type Func, type SampleCounts } from "../src/iongraph.js";
import { must } from "../src/utils.js";
import { E } from "../src/dom.js";
import { GraphViewer } from "../src/GraphViewer.js";

const searchParams = new URL(window.location.toString()).searchParams;

const initialFuncIndex = searchParams.has("func") ? parseInt(searchParams.get("func")!, 10) : undefined;
const initialPass = searchParams.has("pass") ? parseInt(searchParams.get("pass")!, 10) : undefined;

interface MenuBarProps {
  browse?: boolean,
  export?: boolean,
  funcSelected: (func: Func | null) => void,
}

class MenuBar {
  root: HTMLElement;
  funcSelector: HTMLElement;
  funcSelectorNone: HTMLElement;
  funcName: HTMLElement;
  exportButton: HTMLButtonElement | null;

  ionjson: IonJSON | null;
  funcIndex: number;
  funcSelected: (func: Func | null) => void;

  constructor(props: MenuBarProps) {
    this.exportButton = null;

    this.ionjson = null;
    this.funcIndex = initialFuncIndex ?? 0;
    this.funcSelected = props.funcSelected;

    this.funcSelector = E("div", [], () => { }, [
      "Function",
      E("input", ["ig-w3"], input => {
        input.type = "number";
        input.min = "1";
        input.addEventListener("input", () => {
          this.switchFunc(parseInt(input.value, 10) - 1);
        });
      }, []),
      " / ",
      E("span", ["num-functions"]),
    ]);
    this.funcSelectorNone = E("div", [], () => { }, ["No functions to display."]);
    this.funcName = E("div");
    this.root = E("div", ["ig-bb", "ig-flex", "ig-bg-white"], () => { }, [
      E("div", ["ig-pv2", "ig-ph3", "ig-flex", "ig-g2", "ig-items-center", "ig-br", "ig-hide-if-empty"], () => { }, [
        props.browse && E("div", [], () => { }, [
          E("input", [], input => {
            input.type = "file";
            input.addEventListener("change", e => {
              const input = e.target as HTMLInputElement;
              if (!input.files?.length) {
                return;
              }
              this.fileSelected(input.files[0]);
            });
          }),
        ]),
        this.funcSelector,
        this.funcSelectorNone,
      ]),
      E("div", ["ig-flex-grow-1", "ig-pv2", "ig-ph3", "ig-flex", "ig-g2", "ig-items-center"], () => { }, [
        this.funcName,
        E("div", ["ig-flex-grow-1"]),
        props.export && E("div", [], () => { }, [
          E("button", [], button => {
            this.exportButton = button;
            button.addEventListener("click", () => {
              this.exportStandalone();
            });
          }, ["Export"]),
        ]),
      ]),
    ]);

    this.update();
  }

  async fileSelected(file: File) {
    const newJSON = JSON.parse(await file.text());
    this.ionjson = migrate(newJSON);
    this.switchFunc(0);
    this.update();
  }

  switchIonJSON(ionjson: IonJSON) {
    this.ionjson = ionjson;
    this.switchFunc(this.funcIndex);
  }

  switchFunc(funcIndex: number) {
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

    const funcInput = this.funcSelector.querySelector("input")!;
    funcInput.max = `${this.numFunctions()}`;
    funcInput.value = `${this.funcIndex + 1}`;
    this.funcSelector.querySelector(".num-functions")!.innerHTML = `${this.numFunctions()}`;

    this.funcName.hidden = !funcIndexValid;
    this.funcName.innerText = `${this.ionjson?.functions[this.funcIndex].name ?? ""}`;

    if (this.exportButton) {
      this.exportButton.disabled = !this.ionjson || !funcIndexValid;
    }
  }

  async exportStandalone() {
    const ion = must(this.ionjson);
    const name = ion.functions[this.funcIndex].name;
    const result: IonJSON = { version: 1, functions: [ion.functions[this.funcIndex]] };

    // Use embedded template if available (file:// mode), otherwise fetch
    const template = (window as any).__standaloneTemplate
      ?? await (await fetch("./standalone.html")).text();
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
}

export class WebUI {
  root: HTMLElement;
  menuBar: MenuBar;
  graphContainer: HTMLElement;

  func: Func | null;
  sampleCountsFromFile: SampleCounts | undefined;
  graph: GraphViewer | null;

  constructor() {
    this.menuBar = new MenuBar({
      browse: true,
      export: true,
      funcSelected: f => this.switchFunc(f),
    });

    this.func = null;
    this.sampleCountsFromFile = undefined;
    this.graph = null;

    this.loadStuffFromQueryParams();

    this.graphContainer = E("div", ["ig-relative", "ig-flex-basis-0", "ig-flex-grow-1", "ig-overflow-hidden"]);
    this.root = E("div", ["ig-absolute", "ig-absolute-fill", "ig-flex", "ig-flex-column"], root => {
      root.addEventListener("dragenter", e => {
        e.preventDefault();
        e.stopPropagation();
      });

      root.addEventListener("dragover", e => {
        e.preventDefault();
        e.stopPropagation();
      });

      root.addEventListener("drop", e => {
        e.preventDefault();
        e.stopPropagation();

        const files = e.dataTransfer?.files;
        if (files && files.length > 0) {
          this.menuBar.fileSelected(files[0]);
        }
      });
    }, [
      this.menuBar.root,
      this.graphContainer,
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
        sampleCounts: this.sampleCountsFromFile,
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
        this.menuBar.switchIonJSON(migrated); // will call funcSelected
      }
    })();
    (async () => {
      const sampleCountsFile = searchParams.get("sampleCounts");
      if (sampleCountsFile) {
        const res = await fetch(sampleCountsFile);
        const json = await res.json();
        this.sampleCountsFromFile = {
          selfLineHits: new Map(json["selfLineHits"]),
          totalLineHits: new Map(json["totalLineHits"]),
        };
        this.update();
      }
    })();
  }

  switchFunc(func: Func | null) {
    this.func = func;
    this.update();
  }
}

export class StandaloneUI {
  root: HTMLElement;
  menuBar: MenuBar;
  graphContainer: HTMLElement;

  func: Func | null;
  graph: GraphViewer | null;

  constructor() {
    this.menuBar = new MenuBar({
      funcSelected: f => this.switchFunc(f),
    });

    this.func = null;
    this.graph = null;

    this.graphContainer = E("div", ["ig-relative", "ig-flex-basis-0", "ig-flex-grow-1", "ig-overflow-hidden"]);
    this.root = E("div", ["ig-absolute", "ig-absolute-fill", "ig-flex", "ig-flex-column"], () => { }, [
      this.menuBar.root,
      this.graphContainer,
    ]);
  }

  update() {
    if (this.graph) {
      this.graph.destroy();
    }
    if (this.func) {
      this.graph = new GraphViewer(this.graphContainer, {
        func: this.func,
        pass: initialPass,
      });
    }
  }

  setIonJSON(ion: IonJSON) {
    this.menuBar.switchIonJSON(ion)
  }

  switchFunc(func: Func | null) {
    this.func = func;
    this.update();
  }
}
