export const currentVersion = 2 as const;

export interface IonJSON {
  version: typeof currentVersion,
  functions: Func[],
}

export interface Func {
  name: string,
  tier: string | null,
  osr: boolean,
  passes: Pass[],
}

export interface Pass {
  name: string,
  mir: {
    blocks: MIRBlock[],
  },
  lir: {
    blocks: LIRBlock[],
  },
}

export type BlockPtr = number & { readonly __brand: "BlockPtr" }
export type BlockID = number & { readonly __brand: "BlockID" }
export type InsPtr = number & { readonly __brand: "InsPtr" }
export type InsID = number & { readonly __brand: "InsID" }

export interface MIRBlock {
  ptr: BlockPtr,
  id: BlockID,
  loopDepth: number,
  attributes: string[],
  predecessors: BlockID[],
  successors: BlockID[],
  instructions: MIRInstruction[],
}

export interface MIRInstruction {
  ptr: InsPtr,
  id: InsID,
  opcode: string,
  attributes: string[],
  inputs: number[],
  uses: number[],
  memInputs: unknown[], // TODO
  type: string,
}

export interface LIRBlock {
  ptr: BlockPtr,
  id: BlockID,
  instructions: LIRInstruction[],
}

export interface LIRInstruction {
  ptr: InsPtr,
  id: InsID,
  mirPtr: number | null,
  opcode: string,
  defs: number[],
}

export interface SampleCounts {
  selfLineHits: Map<number, number>,
  totalLineHits: Map<number, number>,
}

/**
 * Migrate ion JSON data to the latest version of the schema. A history of
 * schema changes can be found at the end of the file.
 */
export function migrate(ionJSON: any): IonJSON {
  if (ionJSON.version === undefined) {
    ionJSON.version = 0;
  }

  for (const f of ionJSON.functions) {
    migrateFunc(f, ionJSON.version);
  }

  ionJSON.version = currentVersion;
  return ionJSON;
}

function migrateFunc(f: any, version: number): Func {
  if (version < 2) {
    if (f.tier === undefined) {
      f.tier = null;
    }
    if (f.osr === undefined) {
      f.osr = false;
    }
  }

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

function migrateMIRBlock(b: any, version: number): MIRBlock {
  if (version === 0) {
    b.ptr = ((b.id ?? b.number) + 1) as any as BlockPtr;
    b.id = b.number;
  }

  for (const ins of b.instructions) {
    migrateMIRInstruction(ins, version);
  }

  return b;
}

function migrateMIRInstruction(ins: any, version: number): MIRInstruction {
  if (version === 0) {
    ins.ptr = ins.id;
  }

  return ins;
}

function migrateLIRBlock(b: any, version: number): MIRBlock {
  if (version === 0) {
    b.ptr = (b.id ?? b.number) as any as BlockPtr;
    b.id = b.number;
  }

  for (const ins of b.instructions) {
    migrateLIRInstruction(ins, version);
  }

  return b;
}

function migrateLIRInstruction(ins: any, version: number): LIRInstruction {
  if (version === 0) {
    ins.ptr = ins.id;
    ins.mirPtr = null;
  }

  return ins;
}

/*
# History of the ion.json schema

- Version 0: "Legacy" ion.json as used by sstangl's iongraph tool. Never
  explicitly versioned.

- Version 1: Created for the release of the web-based iongraph tool. The first
  explicitly-versioned schema. Key changes:
  - Renamed "number" to "id" on MIR and LIR blocks for consistency with C++.
  - Added "ptr" to MIR blocks and MIR and LIR instructions for stable
    identification across passes. LIR blocks do not need this because they are
    stably identified by their corresponding MIR block.
  - Added "mirPtr" to LIR instructions so that they can be traced back to their
    MIR instruction. May be null.

- Version 2: Added compilation tier metadata to functions:
  - Added "tier" on Func: a string identifying the compilation tier (e.g.
    "DFG", "FTL", "OMG"). May be null when unknown.
  - Added "osr" on Func: true when the compilation is an OSR-entry variant
    (FTLForOSREntry, OMGForOSREntry).

*/