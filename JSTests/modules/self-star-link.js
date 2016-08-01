//       +-------------+
//       |             |
//       v             |
//  @-> (A) -> (B) -> (C) *-> [E]
//       *
//       |
//       v
//      [D]
import { A } from "./self-star-link/A.js"
import { shouldBe } from "./resources/assert.js"
import { A as AfromC } from "./self-star-link/C.js"
import { A as AfromB } from "./self-star-link/B.js"
shouldBe(A, 'E');
shouldBe(AfromC, 'D');
shouldBe(AfromB, 'D');
