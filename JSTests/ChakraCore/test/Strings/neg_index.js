//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//reduced switches: -maxsimplejitruncount:1 -maxinterpretcount:1

var zibhbj;

for (i=0; i<10000; i++) {
   zibhbj = 'u{3402e}'[-1];
}
if (zibhbj === undefined) 
{
    WScript.Echo("Passed");    
}
else
{
    WScript.Echo("FAILED");
}
