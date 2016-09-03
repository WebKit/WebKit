//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var DEBUG = false;
function sameValue(x, y) {
  if (x == y)
  {
     return x != 0 || y != 0 || (1/x == 1/y); // 0.0 != -0.0
  }
  return (x != x) && (y != y);  // NaN == NaN
}

function equal(ev, v) {
    var eps = 0.000001;

    if (sameValue(ev, v))
    {
        if(DEBUG) WScript.Echo("same value");
        return true;
    }
    else if ((ev == 0.0 || v == 0.0) && Math.abs(v - ev) <= eps) // -0.0 covered here
    {
        if(DEBUG) WScript.Echo("Float 0.0 ");
        return true;
    }
    else if (Math.abs(v - ev) / Math.abs(ev) <= eps)
    {
        if(DEBUG) WScript.Echo("Float values within eps");
        return true;
    }
    else
    {
        WScript.Echo(">> Fail!");
        WScript.Echo("Expected "+ev+" Found "+v);
        return false;
    }
}

function equalSimd(values, simdValue, type, msg)
{   var ok = true;
    
    length = getLength(type);
    
    if (Array.isArray(values))
    {
        for ( var i = 0; i < length; i ++)
        {
            if(!equal(values[i],type.extractLane(simdValue, i)))
            {
                ok = false;
            }
        }
        if (ok)
            return;
        else
        {
            
            WScript.Echo(">> Fail!!");
            if (msg !== undefined)
            {
                WScript.Echo(msg);
                
            }
            printSimd(simdValue, type);
        }
    }
    else
    {
        type.check(values);
        
        for ( var i = 0; i < length; i ++)
        {
            if(!equal(type.extractLane(values, i),type.extractLane(simdValue, i)))
            {
                ok = false;
            }
        }
        if (ok)
            return;
        else
        {
            WScript.Echo(">> Fail!!");
            if (msg !== undefined)
            {
                WScript.Echo(msg);
            }
            printSimd(simdValue, type);
        }
    }
}

function printSimd(simdValue, type)
{
    var length;
    var vals = "";
    
    length = getLength(type);
    
    for (var i = 0; i < length; i ++)
    {
        vals += type.extractLane(simdValue,i);
        if (i < length - 1)
            vals += ", "
    }
    WScript.Echo(type.toString() + "(" + vals + ")");
}

function printSimdBaseline(simdValue, typeName, varName, msg)
{
    var length;
    var vals = "";
    
    if (typeName === "SIMD.Float32x4")
    {
        type = SIMD.Float32x4;
    }
    else if (typeName === "SIMD.Int32x4")
    {
        type = SIMD.Int32x4;
        
    }
    else if (typeName === "SIMD.Int16x8")
    {
        type = SIMD.Int16x8;
        
    }
    else if (typeName === "SIMD.Int8x16")
    {
        type = SIMD.Int8x16;
    }
    else if (typeName === "SIMD.Uint32x4")
    {
        type = SIMD.Uint32x4;
    }
    else if (typeName === "SIMD.Uint16x8")
    {
        type = SIMD.Uint16x8;
    }
    else if (typeName === "SIMD.Uint8x16")
    {
        type = SIMD.Uint8x16;
    }
    else if (typeName === "SIMD.Bool32x4")
    {
        type = SIMD.Bool32x4;
    }
    else if (typeName === "SIMD.Bool16x8")
    {
        type = SIMD.Bool16x8;
    }
    else if (typeName === "SIMD.Bool8x16")
    {
        type = SIMD.Bool8x16;
    }
    else
    {
        throw "Unsupported type";

    }
    
    length = getLength(type);
    
    for (var i = 0; i < length; i ++)
    {
        vals += type.extractLane(simdValue,i);
        if (i < length - 1)
            vals += ", "
    }
   print("equalSimd([" + vals + "], " + varName + ", " + typeName + ", \"" + msg + "\")");
   
}

function getLength(type)
{
    var length;
    switch (type)
    {
        case SIMD.Float32x4:
        case SIMD.Int32x4:
        case SIMD.Uint32x4:
        case SIMD.Bool32x4:
            length = 4;
            break;
        case SIMD.Int16x8:
        case SIMD.Uint16x8:
        case SIMD.Bool16x8:
            length = 8;
            break;
        case SIMD.Int8x16:
        case SIMD.Uint8x16:
        case SIMD.Bool8x16:
            length = 16;
            break;
        default:
            throw "Undefined type";
    }
    return length;
}
