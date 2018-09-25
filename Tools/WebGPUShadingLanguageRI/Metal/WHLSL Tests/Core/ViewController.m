/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "ViewController.h"
#import "OnscreenRenderer.h"

@interface ViewController ()

@property OnscreenRenderer *renderer;
@property NSMagnificationGestureRecognizer *magnifyGesture;
@property NSRotationGestureRecognizer *rotationGesture;
@property (weak) IBOutlet NSTextField *debugLabel;
@property (weak) IBOutlet NSScrollView *whlslViewContainer;
@property (weak) IBOutlet NSScrollView *mslViewContainer;
@property id<MTLLibrary> library;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    [self loadFile:@"Default"];

    self.compiler = [[Compiler alloc] init];

    self.metalView.device = MTLCreateSystemDefaultDevice();
    self.renderer = [[OnscreenRenderer alloc] initWithView:self.metalView device:self.metalView.device];

    self.magnifyGesture = [[NSMagnificationGestureRecognizer alloc] initWithTarget:self action:@selector(zoom:)];
    self.magnifyGesture.delegate = self;
    [self.metalView addGestureRecognizer:self.magnifyGesture];

    self.rotationGesture = [[NSRotationGestureRecognizer alloc] initWithTarget:self action:@selector(rotate:)];
    self.rotationGesture.delegate = self;
    [self.metalView addGestureRecognizer:self.rotationGesture];

    self.metalView.scrollDelegate = self;

    [self viewDidLayout];

    self.library = [self.renderer.device newDefaultLibraryWithBundle:[NSBundle mainBundle] error:nil];
}

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(nonnull NSGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

- (void)viewDidLayout
{
    [super viewDidLayout];

    CGFloat width = CGRectGetWidth(self.view.frame);
    CGFloat boxWidth = (width - 20 * 2 - 10 * 2) / 3;
    CGFloat height = CGRectGetHeight(self.view.frame);
    CGFloat boxHeight = height - 60 - 20;

    self.whlslViewContainer.frame = CGRectMake(20, 60, boxWidth, boxHeight);
    self.mslViewContainer.frame = CGRectMake(20 + boxWidth + 10, 60, boxWidth, boxHeight);
    self.metalView.frame = CGRectMake(20 + boxWidth + 10 + boxWidth + 10, 60, boxWidth, boxHeight);

    float ratio = boxHeight / (boxWidth / 3);
    // NOTE: simd_matrix takes columns, not rows!
    self.renderer.transformMatrix = simd_matrix(simd_make_float3(3, 0, 0), simd_make_float3(0,  ratio, 0), simd_make_float3(-2.2, -ratio / 2, 1));

    self.debugLabel.stringValue = [NSString stringWithFormat:@"Width = %f, height = %f", boxWidth, boxHeight];
}

- (IBAction)resetViewport:(id)sender
{
    [self viewDidLayout];
    [self.metalView setNeedsDisplay:YES];
}

- (IBAction)loadDefault:(id)sender
{
    [self loadFile:@"Default"];
}

- (IBAction)loadMandelbrot:(id)sender
{
    [self loadFile:@"Mandelbrot"];
}

- (IBAction)loadJulia:(id)sender
{
    [self loadFile:@"Julia"];
}

- (void)loadFile:(NSString*)name
{
    NSString* defaultFilePath = [[NSBundle mainBundle] pathForResource:name ofType:@"whlsl"];
    NSString* defaultContents = [NSString stringWithContentsOfFile:defaultFilePath encoding:NSUTF8StringEncoding error:nil];

    // Do any additional setup after loading the view.
    self.whlslTextView.string = defaultContents;
    self.mslTextView.string = @"";
    self.metalView.hidden = YES;
}

- (IBAction)compileClicked:(id)sender {
    NSError *error = nil;
    CompileResult *output = [self.compiler compileWhlslToMetal:self.whlslTextView.string error:&error];
    if (error)
        self.mslTextView.string = error.localizedDescription;
    else {
        self.metalView.hidden = NO;
        self.mslTextView.string = output.source;
        NSString *vertexShaderName = output.functionNameMap[@"vertexShader"];
        NSString *fragmentShaderName = output.functionNameMap[@"fragmentShader"];
        [self viewDidLayout];
        [self.renderer setShaderSource:output.source vertexShaderName:vertexShaderName fragmentShaderName:fragmentShaderName];
        [self.metalView setNeedsDisplay:YES];
    }
    self.mslTextView.font = [NSFont fontWithName:@"Menlo" size:12];
}

- (IBAction)loadNativeFlatColor:(id)sender
{
    [self loadNativeFlatColor:@"flatColor"];
}

- (IBAction)loadNativeMandelbrot:(id)sender
{
    [self loadNativeMetalShaderWithName:@"mandelbrot"];
}

- (IBAction)loadNativeJulia:(id)sender
{
    [self loadNativeMetalShaderWithName:@"julia"];
}

- (void)loadNativeMetalShaderWithName:(NSString*)name
{
    NSString* vertexShaderName = [name stringByAppendingString:@"VertexShader"];
    NSString* fragmentShaderName = [name stringByAppendingString:@"FragmentShader"];
    [self viewDidLayout];
    [self.renderer setLibrary:self.library vertexShaderName:vertexShaderName fragmentShaderName:fragmentShaderName pixelFormat:self.metalView.colorPixelFormat];
    [self.metalView setNeedsDisplay:YES];
}

- (IBAction)zoom:(id)sender
{
    float factor = 1.0f / (1 + self.magnifyGesture.magnification);
    NSPoint location = [self.magnifyGesture locationInView:self.metalView];
    float normalizedX = location.x / CGRectGetWidth(self.metalView.frame);
    float normalizedY = location.y / CGRectGetHeight(self.metalView.frame);
    simd_float3 normalized = simd_make_float3(normalizedX, normalizedY, 1);
    normalized = matrix_multiply(self.renderer.transformMatrix, normalized);
    simd_float3x3 initTransform = simd_matrix(simd_make_float3(1, 0, 0), simd_make_float3(0, 1, 0), simd_make_float3(-normalized.x, -normalized.y, 1));
    simd_float3x3 scaleMatrix = simd_matrix(simd_make_float3(factor, 0.f, 0.f), simd_make_float3(0.f, factor, 0.f), simd_make_float3(0.f, 0.f, 1));
    simd_float3x3 backTransform = simd_matrix(simd_make_float3(1, 0, 0), simd_make_float3(0, 1, 0), simd_make_float3(normalized.x, normalized.y, 1));
    self.renderer.transformMatrix = matrix_multiply(matrix_multiply(backTransform, matrix_multiply(scaleMatrix, initTransform)), self.renderer.transformMatrix);
    self.magnifyGesture.magnification = 0;
    [self.metalView setNeedsDisplay:YES];
}

- (IBAction)rotate:(id)sender
{
    float angle = -self.rotationGesture.rotation;
    NSPoint location = [self.rotationGesture locationInView:self.metalView];
    float normalizedX = location.x / CGRectGetWidth(self.metalView.frame);
    float normalizedY = location.y / CGRectGetHeight(self.metalView.frame);
    simd_float3 normalized = simd_make_float3(normalizedX, normalizedY, 1);
    normalized = matrix_multiply(self.renderer.transformMatrix, normalized);

    simd_float3x3 initTransform = simd_matrix(simd_make_float3(1, 0, 0), simd_make_float3(0, 1, 0), simd_make_float3(-normalized.x, -normalized.y, 1));
    simd_float3x3 rotationMatrix = simd_matrix(simd_make_float3(cos(angle), sin(angle), 0.f), simd_make_float3(-sin(angle), cos(angle), 0.f), simd_make_float3(0.f, 0.f, 1));
    simd_float3x3 backTransform = simd_matrix(simd_make_float3(1, 0, 0), simd_make_float3(0, 1, 0), simd_make_float3(normalized.x, normalized.y, 1));
    self.renderer.transformMatrix = matrix_multiply(matrix_multiply(backTransform, matrix_multiply(rotationMatrix, initTransform)), self.renderer.transformMatrix);
    self.rotationGesture.rotation = 0;
    [self.metalView setNeedsDisplay:YES];
}

- (void)customMetalView:(CustomMetalView *)metalView scrolledDeltaX:(CGFloat)deltaX deltaY:(CGFloat)deltaY
{
    float width = self.metalView.bounds.size.width;
    float height = self.metalView.bounds.size.height;
    float scrollVelocity = 6;
    float x = -deltaX / width * scrollVelocity;
    float y = deltaY / height * scrollVelocity;
    simd_float3x3 m = simd_matrix(simd_make_float3(1, 0, 0), simd_make_float3(0, 1, 0), simd_make_float3(x, y, 1));
    self.renderer.transformMatrix = matrix_multiply(self.renderer.transformMatrix, m);
}

@end
