import { shouldBe, shouldNotBe } from "../resources/assert.js";
import * as self1 from './namespace-re-export.js';
import * as other1 from './namespace-re-export-fixture.js';
import { namespace } from './namespace-re-export-fixture.js';

// Re-exported namespace objects
shouldBe(self1, namespace);
shouldNotBe(self1, other1);

// Re-exported namespace binding should reside in the namespace-re-export-fixture's namespace object.
shouldBe('namespace' in other1, true);
