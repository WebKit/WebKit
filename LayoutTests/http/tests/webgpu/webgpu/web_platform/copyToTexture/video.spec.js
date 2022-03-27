/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
copyToTexture with HTMLVideoElement (and other video-type sources?).

- videos with various encodings, color spaces, metadata

TODO: consider whether external_texture and copyToTexture video tests should be in the same file
TODO: plan
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { GPUTest } from '../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
