//@ skip if $disabledKeys&.include?(:TestKey)
//@ runDefault

if (Math.max(1, 2) !== 2)
    throw new Error("bad result: " + Math.max(1, 2));
