/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { globalTestConfig } from '../../framework/test_config.js';import { Logger } from '../../internal/logging/logger.js';
import { setDefaultRequestAdapterOptions } from '../../util/navigator_gpu.js';









/**
 * Set config environment for workers with ctsOptions and return a Logger.
 */
export function setupWorkerEnvironment(ctsOptions) {
  const { debug, unrollConstEvalLoops, powerPreference, compatibility } = ctsOptions;
  globalTestConfig.unrollConstEvalLoops = unrollConstEvalLoops;
  globalTestConfig.compatibility = compatibility;

  Logger.globalDebugMode = debug;
  const log = new Logger();

  if (powerPreference || compatibility) {
    setDefaultRequestAdapterOptions({
      ...(powerPreference && { powerPreference }),
      // MAINTENANCE_TODO: Change this to whatever the option ends up being
      ...(compatibility && { compatibilityMode: true })
    });
  }

  return log;
}