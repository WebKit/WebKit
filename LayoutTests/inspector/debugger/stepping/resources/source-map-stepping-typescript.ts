function compute(value: number): number {
    debugger;
    log(value); log(value + 1);
    return value * 2;
}

function log(value: number): void {
    (self as any).sink = value;
}

(self as any).compute = compute;
