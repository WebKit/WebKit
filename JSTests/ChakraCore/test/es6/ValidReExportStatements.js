//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Re-export statements
export * from 'ValidExportDefaultStatement1.js';
export {} from 'ValidExportDefaultStatement2.js';
export { foo } from 'ValidExportStatements.js';
export { bar, } from 'ValidExportStatements.js';
export { foo as foo2, baz } from 'ValidExportStatements.js'
export { foo as foo3, bar as bar2, } from 'ValidExportStatements.js'
