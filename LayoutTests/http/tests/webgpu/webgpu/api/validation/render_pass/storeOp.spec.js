/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
API Validation Tests for RenderPass StoreOp.

Test Coverage:
  - Tests that when depthReadOnly is true, depthStoreOp must be 'store'.
    - When depthReadOnly is true and depthStoreOp is 'discard', an error should be generated.

  - Tests that when stencilReadOnly is true, stencilStoreOp must be 'store'.
    - When stencilReadOnly is true and stencilStoreOp is 'discard', an error should be generated.

  - Tests that the depthReadOnly value matches the stencilReadOnly value.
    - When depthReadOnly does not match stencilReadOnly, an error should be generated.

  - Tests that depthReadOnly and stencilReadOnly default to false.

TODO: test interactions with depthClearValue too
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('store_op_and_read_only')
  .paramsSimple([
    { readonly: true, _valid: true },
    // Using depthReadOnly=true and depthStoreOp='discard' should cause a validation error.
    { readonly: true, depthStoreOp: 'discard', _valid: false },
    // Using stencilReadOnly=true and stencilStoreOp='discard' should cause a validation error.
    { readonly: true, stencilStoreOp: 'discard', _valid: false },
    // Mismatched depthReadOnly and stencilReadOnly values should cause a validation error.
    { readonly: false, _valid: true },
    { readonly: false, depthReadOnly: true, _valid: false },
    { readonly: false, stencilReadOnly: true, _valid: false },
    // depthReadOnly and stencilReadOnly should default to false.
    { readonly: undefined, _valid: true },
    { readonly: undefined, depthReadOnly: true, _valid: false },
    { readonly: undefined, stencilReadOnly: true, _valid: false },
  ])
  .fn(t => {
    const {
      readonly,
      depthStoreOp = 'store',
      depthReadOnly = readonly,
      stencilStoreOp = 'store',
      stencilReadOnly = readonly,
      _valid,
    } = t.params;

    const depthAttachment = t.device.createTexture({
      format: 'depth24plus-stencil8',
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });
    const depthAttachmentView = depthAttachment.createView();

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [],
      depthStencilAttachment: {
        view: depthAttachmentView,
        depthLoadOp: 'load',
        depthStoreOp,
        depthReadOnly,
        stencilLoadOp: 'load',
        stencilStoreOp,
        stencilReadOnly,
      },
    });
    pass.end();

    t.expectValidationError(() => {
      encoder.finish();
    }, !_valid);
  });
