/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import * as fs from 'fs';import * as process from 'process';
import { DefaultTestFileLoader } from '../internal/file_loader.js';
import { Ordering, compareQueries } from '../internal/query/compare.js';
import { parseQuery } from '../internal/query/parseQuery.js';
import { TestQueryMultiFile } from '../internal/query/query.js';
import { loadTreeForQuery } from '../internal/tree.js';
import { StacklessError } from '../internal/util.js';
import { assert } from '../util/util.js';

function usage(rc) {
  console.error('Usage:');
  console.error('  tools/checklist FILE');
  console.error('  tools/checklist my/list.txt');
  process.exit(rc);
}

if (process.argv.length === 2) usage(0);
if (process.argv.length !== 3) usage(1);




async function loadQueryListFromTextFile(filename) {
  const lines = (await fs.promises.readFile(filename, 'utf8')).split(/\r?\n/);
  const allQueries = lines.
  filter((l) => l).
  map((l) => {
    const [doneStr, q] = l.split(/\s+/);
    assert(doneStr === 'DONE' || doneStr === 'TODO', 'first column must be DONE or TODO');
    return { query: parseQuery(q), done: doneStr === 'DONE' };
  });

  const queriesBySuite = new Map();
  for (const q of allQueries) {
    let suiteQueries = queriesBySuite.get(q.query.suite);
    if (suiteQueries === undefined) {
      suiteQueries = [];
      queriesBySuite.set(q.query.suite, suiteQueries);
    }

    suiteQueries.push(q);
  }

  return queriesBySuite;
}

function checkForOverlappingQueries(queries) {
  for (let i1 = 0; i1 < queries.length; ++i1) {
    for (let i2 = i1 + 1; i2 < queries.length; ++i2) {
      const q1 = queries[i1].query;
      const q2 = queries[i2].query;
      if (compareQueries(q1, q2) !== Ordering.Unordered) {
        console.log(`    FYI, the following checklist items overlap:\n      ${q1}\n      ${q2}`);
      }
    }
  }
}

function checkForUnmatchedSubtreesAndDoneness(
tree,
matchQueries)
{
  let subtreeCount = 0;
  const unmatchedSubtrees = [];
  const overbroadMatches = [];
  const donenessMismatches = [];
  const alwaysExpandThroughLevel = 1; // expand to, at minimum, every file.
  for (const subtree of tree.iterateCollapsedNodes({
    includeIntermediateNodes: true,
    includeEmptySubtrees: true,
    alwaysExpandThroughLevel
  })) {
    subtreeCount++;
    const subtreeDone = !subtree.subtreeCounts?.nodesWithTODO;

    let subtreeMatched = false;
    for (const q of matchQueries) {
      const comparison = compareQueries(q.query, subtree.query);
      if (comparison !== Ordering.Unordered) subtreeMatched = true;
      if (comparison === Ordering.StrictSubset) continue;
      if (comparison === Ordering.StrictSuperset) overbroadMatches.push([q.query, subtree.query]);
      if (comparison === Ordering.Equal && q.done !== subtreeDone) donenessMismatches.push(q);
    }
    if (!subtreeMatched) unmatchedSubtrees.push(subtree.query);
  }

  if (overbroadMatches.length) {
    // (note, this doesn't show ALL multi-test queries - just ones that actually match any .spec.ts)
    console.log(`  FYI, the following checklist items were broader than one file:`);
    for (const [q, collapsedSubtree] of overbroadMatches) {
      console.log(`    ${q}  >  ${collapsedSubtree}`);
    }
  }

  if (unmatchedSubtrees.length) {
    throw new StacklessError(`Found unmatched tests:\n  ${unmatchedSubtrees.join('\n  ')}`);
  }

  if (donenessMismatches.length) {
    throw new StacklessError(
    'Found done/todo mismatches:\n  ' +
    donenessMismatches.
    map((q) => `marked ${q.done ? 'DONE, but is TODO' : 'TODO, but is DONE'}: ${q.query}`).
    join('\n  '));

  }

  return subtreeCount;
}

(async () => {
  console.log('Loading queries...');
  const queriesBySuite = await loadQueryListFromTextFile(process.argv[2]);
  console.log('  Found suites: ' + Array.from(queriesBySuite.keys()).join(' '));

  const loader = new DefaultTestFileLoader();
  for (const [suite, queriesInSuite] of queriesBySuite.entries()) {
    console.log(`Suite "${suite}":`);
    console.log(`  Checking overlaps between ${queriesInSuite.length} checklist items...`);
    checkForOverlappingQueries(queriesInSuite);
    const suiteQuery = new TestQueryMultiFile(suite, []);
    console.log(`  Loading tree ${suiteQuery}...`);
    const tree = await loadTreeForQuery(
    loader,
    suiteQuery,
    queriesInSuite.map((q) => q.query));

    console.log('  Found no invalid queries in the checklist. Checking for unmatched tests...');
    const subtreeCount = checkForUnmatchedSubtreesAndDoneness(tree, queriesInSuite);
    console.log(`  No unmatched tests or done/todo mismatches among ${subtreeCount} subtrees!`);
  }
  console.log(`Checklist looks good!`);
})().catch((ex) => {
  console.log(ex.stack ?? ex.toString());
  process.exit(1);
});
//# sourceMappingURL=checklist.js.map