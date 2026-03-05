/**
 * DOM utilities to ease the pain of document.createElement.
 */

type NonNull<T> = T extends null ? never : T;
type CopyNullable<Src, Dst> = Src extends null ? Dst | null : NonNull<Dst>;
type Falsy = null | undefined | false;

/**
 * A slightly relaxed Node type for my DOM utilities.
 */
export type BNode = Node | string | Falsy;

/**
 * Ensures a DOM Node.
 */
export function N<T extends BNode>(v: T): CopyNullable<T, Node> {
  if (typeof v === "string") {
    return document.createTextNode(v);
  }
  return v as any;
}

/**
 * Adds children to a DOM node.
 */
export function addChildren(n: Node, children: BNode[]) {
  for (const child of children) {
    if (child) {
      n.appendChild(N(child));
    }
  }
}

export function E<K extends keyof HTMLElementTagNameMap>(
  type: K,
  classes?: (string | Falsy)[],
  init?: (el: HTMLElementTagNameMap[K]) => void,
  children?: BNode[],
): HTMLElementTagNameMap[K];

export function E(
  type: string,
  classes?: (string | Falsy)[],
  init?: (el: HTMLElement) => void,
  children?: BNode[],
): HTMLElement;

/**
 * Creates a DOM element.
 * @param type The type of DOM element to create (e.g. `"div"`)
 * @param classes Any classes to add to the element
 * @param children Any children to add to the element
 */
export function E(
  type: string,
  classes?: (string | Falsy)[],
  init?: (el: HTMLElement) => void,
  children?: BNode[],
) {
  const el = document.createElement(type);
  if (classes && classes.length > 0) {
    const actualClasses: string[] = classes.filter((c): c is string => !!c);
    el.classList.add(...actualClasses);
  }
  init?.(el);
  if (children) {
    addChildren(el, children);
  }
  return el;
}

/**
 * Creates a DOM fragment.
 * @param children
 */
export function F(children: BNode[]): DocumentFragment {
  const f = document.createDocumentFragment();
  addChildren(f, children);
  return f;
}
