import { B } from "./module-simple-B.mjs";
debug("Module A was imported.");

if (B == 42)
    debug("Exported B was visible.");
