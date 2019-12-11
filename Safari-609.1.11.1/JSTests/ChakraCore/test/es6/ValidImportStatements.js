//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Empty import statement is legal - does nothing but execute the module
import "ValidExportStatements.js";
import "ValidExportStatements.js"

// Import default binding
import ns1 from "ValidExportStatements.js";
import ns2 from "ValidExportStatements.js"

// Named import list
import { foo } from "ValidExportStatements.js";
import { foo as foo22, bar, } from "ValidExportStatements.js"

// Namespace import statement
import * as ns3 from "ValidExportStatements.js";
import * as ns8 from "ValidExportStatements.js"

// Import statement with default binding and a second clause
//import ns4, * as ns5 from "ValidExportStatements.js"
import ns6, { baz } from "ValidExportStatements.js";
import ns7, { foo as foo23, foobar, } from "ValidExportStatements.js"
