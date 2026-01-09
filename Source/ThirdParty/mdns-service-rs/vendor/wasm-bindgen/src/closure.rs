//! Support for long-lived closures in `wasm-bindgen`
//!
//! This module defines the `Closure` type which is used to pass "owned
//! closures" from Rust to JS. Some more details can be found on the `Closure`
//! type itself.

#![allow(clippy::fn_to_numeric_cast)]

use alloc::boxed::Box;
use alloc::string::String;
use core::fmt;
use core::mem::{self, ManuallyDrop};

use crate::convert::*;
use crate::describe::*;
use crate::JsValue;
use core::marker::PhantomData;

#[wasm_bindgen_macro::wasm_bindgen(wasm_bindgen = crate)]
extern "C" {
    type JsClosure;

    #[wasm_bindgen(method)]
    fn _wbg_cb_unref(js: &JsClosure);
}

/// A handle to both a closure in Rust as well as JS closure which will invoke
/// the Rust closure.
///
/// A `Closure` is the primary way that a `'static` lifetime closure is
/// transferred from Rust to JS. `Closure` currently requires that the closures
/// it's created with have the `'static` lifetime in Rust for soundness reasons.
///
/// This type is a "handle" in the sense that whenever it is dropped it will
/// invalidate the JS closure that it refers to. Any usage of the closure in JS
/// after the `Closure` has been dropped will raise an exception. It's then up
/// to you to arrange for `Closure` to be properly deallocate at an appropriate
/// location in your program.
///
/// The type parameter on `Closure` is the type of closure that this represents.
/// Currently this can only be the `Fn` and `FnMut` traits with up to 7
/// arguments (and an optional return value).
///
/// # Examples
///
/// Here are a number of examples of using `Closure`.
///
/// ## Using the `setInterval` API
///
/// Sample usage of `Closure` to invoke the `setInterval` API.
///
/// ```rust,no_run
/// use wasm_bindgen::prelude::*;
///
/// #[wasm_bindgen]
/// extern "C" {
///     fn setInterval(closure: &Closure<dyn FnMut()>, time: u32) -> i32;
///     fn clearInterval(id: i32);
///
///     #[wasm_bindgen(js_namespace = console)]
///     fn log(s: &str);
/// }
///
/// #[wasm_bindgen]
/// pub struct IntervalHandle {
///     interval_id: i32,
///     _closure: Closure<dyn FnMut()>,
/// }
///
/// impl Drop for IntervalHandle {
///     fn drop(&mut self) {
///         clearInterval(self.interval_id);
///     }
/// }
///
/// #[wasm_bindgen]
/// pub fn run() -> IntervalHandle {
///     // First up we use `Closure::new` to wrap up a Rust closure and create
///     // a JS closure.
///     let cb = Closure::new(|| {
///         log("interval elapsed!");
///     });
///
///     // Next we pass this via reference to the `setInterval` function, and
///     // `setInterval` gets a handle to the corresponding JS closure.
///     let interval_id = setInterval(&cb, 1_000);
///
///     // If we were to drop `cb` here it would cause an exception to be raised
///     // whenever the interval elapses. Instead we *return* our handle back to JS
///     // so JS can decide when to cancel the interval and deallocate the closure.
///     IntervalHandle {
///         interval_id,
///         _closure: cb,
///     }
/// }
/// ```
///
/// ## Casting a `Closure` to a `js_sys::Function`
///
/// This is the same `setInterval` example as above, except it is using
/// `web_sys` (which uses `js_sys::Function` for callbacks) instead of manually
/// writing bindings to `setInterval` and other Web APIs.
///
/// ```rust,ignore
/// use wasm_bindgen::JsCast;
///
/// #[wasm_bindgen]
/// pub struct IntervalHandle {
///     interval_id: i32,
///     _closure: Closure<dyn FnMut()>,
/// }
///
/// impl Drop for IntervalHandle {
///     fn drop(&mut self) {
///         let window = web_sys::window().unwrap();
///         window.clear_interval_with_handle(self.interval_id);
///     }
/// }
///
/// #[wasm_bindgen]
/// pub fn run() -> Result<IntervalHandle, JsValue> {
///     let cb = Closure::new(|| {
///         web_sys::console::log_1(&"interval elapsed!".into());
///     });
///
///     let window = web_sys::window().unwrap();
///     let interval_id = window.set_interval_with_callback_and_timeout_and_arguments_0(
///         // Note this method call, which uses `as_ref()` to get a `JsValue`
///         // from our `Closure` which is then converted to a `&Function`
///         // using the `JsCast::unchecked_ref` function.
///         cb.as_ref().unchecked_ref(),
///         1_000,
///     )?;
///
///     // Same as above.
///     Ok(IntervalHandle {
///         interval_id,
///         _closure: cb,
///     })
/// }
/// ```
///
/// ## Using `FnOnce` and `Closure::once` with `requestAnimationFrame`
///
/// Because `requestAnimationFrame` only calls its callback once, we can use
/// `FnOnce` and `Closure::once` with it.
///
/// ```rust,no_run
/// use wasm_bindgen::prelude::*;
///
/// #[wasm_bindgen]
/// extern "C" {
///     fn requestAnimationFrame(closure: &Closure<dyn FnMut()>) -> u32;
///     fn cancelAnimationFrame(id: u32);
///
///     #[wasm_bindgen(js_namespace = console)]
///     fn log(s: &str);
/// }
///
/// #[wasm_bindgen]
/// pub struct AnimationFrameHandle {
///     animation_id: u32,
///     _closure: Closure<dyn FnMut()>,
/// }
///
/// impl Drop for AnimationFrameHandle {
///     fn drop(&mut self) {
///         cancelAnimationFrame(self.animation_id);
///     }
/// }
///
/// // A type that will log a message when it is dropped.
/// struct LogOnDrop(&'static str);
/// impl Drop for LogOnDrop {
///     fn drop(&mut self) {
///         log(self.0);
///     }
/// }
///
/// #[wasm_bindgen]
/// pub fn run() -> AnimationFrameHandle {
///     // We are using `Closure::once` which takes a `FnOnce`, so the function
///     // can drop and/or move things that it closes over.
///     let fired = LogOnDrop("animation frame fired or canceled");
///     let cb = Closure::once(move || drop(fired));
///
///     // Schedule the animation frame!
///     let animation_id = requestAnimationFrame(&cb);
///
///     // Again, return a handle to JS, so that the closure is not dropped
///     // immediately and JS can decide whether to cancel the animation frame.
///     AnimationFrameHandle {
///         animation_id,
///         _closure: cb,
///     }
/// }
/// ```
///
/// ## Converting `FnOnce`s directly into JavaScript Functions with `Closure::once_into_js`
///
/// If we don't want to allow a `FnOnce` to be eagerly dropped (maybe because we
/// just want it to drop after it is called and don't care about cancellation)
/// then we can use the `Closure::once_into_js` function.
///
/// This is the same `requestAnimationFrame` example as above, but without
/// supporting early cancellation.
///
/// ```
/// use wasm_bindgen::prelude::*;
///
/// #[wasm_bindgen]
/// extern "C" {
///     // We modify the binding to take an untyped `JsValue` since that is what
///     // is returned by `Closure::once_into_js`.
///     //
///     // If we were using the `web_sys` binding for `requestAnimationFrame`,
///     // then the call sites would cast the `JsValue` into a `&js_sys::Function`
///     // using `f.unchecked_ref::<js_sys::Function>()`. See the `web_sys`
///     // example above for details.
///     fn requestAnimationFrame(callback: JsValue);
///
///     #[wasm_bindgen(js_namespace = console)]
///     fn log(s: &str);
/// }
///
/// // A type that will log a message when it is dropped.
/// struct LogOnDrop(&'static str);
/// impl Drop for LogOnDrop {
///     fn drop(&mut self) {
///         log(self.0);
///     }
/// }
///
/// #[wasm_bindgen]
/// pub fn run() {
///     // We are using `Closure::once_into_js` which takes a `FnOnce` and
///     // converts it into a JavaScript function, which is returned as a
///     // `JsValue`.
///     let fired = LogOnDrop("animation frame fired");
///     let cb = Closure::once_into_js(move || drop(fired));
///
///     // Schedule the animation frame!
///     requestAnimationFrame(cb);
///
///     // No need to worry about whether or not we drop a `Closure`
///     // here or return some sort of handle to JS!
/// }
/// ```
pub struct Closure<T: ?Sized> {
    js: JsClosure,
    // careful: must be Box<T> not just T because unsized PhantomData
    // seems to have weird interaction with Pin<>
    _marker: PhantomData<Box<T>>,
}

fn _assert_compiles<T>(mut pin: core::pin::Pin<&mut Closure<T>>) {
    let _ = &mut *pin;
}

impl<T> Closure<T>
where
    T: ?Sized + WasmClosure,
{
    /// Creates a new instance of `Closure` from the provided Rust function.
    ///
    /// Note that the closure provided here, `F`, has a few requirements
    /// associated with it:
    ///
    /// * It must implement `Fn` or `FnMut` (for `FnOnce` functions see
    ///   `Closure::once` and `Closure::once_into_js`).
    ///
    /// * It must be `'static`, aka no stack references (use the `move`
    ///   keyword).
    ///
    /// * It can have at most 7 arguments.
    ///
    /// * Its arguments and return values are all types that can be shared with
    ///   JS (i.e. have `#[wasm_bindgen]` annotations or are simple numbers,
    ///   etc.)
    pub fn new<F>(t: F) -> Closure<T>
    where
        F: IntoWasmClosure<T> + 'static,
    {
        Closure::wrap(Box::new(t).unsize())
    }

    /// A more direct version of `Closure::new` which creates a `Closure` from
    /// a `Box<dyn Fn>`/`Box<dyn FnMut>`, which is how it's kept internally.
    pub fn wrap(data: Box<T>) -> Closure<T> {
        Self {
            js: crate::__rt::wbg_cast(OwnedClosure(data)),
            _marker: PhantomData,
        }
    }

    /// Release memory management of this closure from Rust to the JS GC.
    ///
    /// When a `Closure` is dropped it will release the Rust memory and
    /// invalidate the associated JS closure, but this isn't always desired.
    /// Some callbacks are alive for the entire duration of the program or for a
    /// lifetime dynamically managed by the JS GC. This function can be used
    /// to drop this `Closure` while keeping the associated JS function still
    /// valid.
    ///
    /// If the platform supports weak references, the Rust memory will be
    /// reclaimed when the JS closure is GC'd. If weak references is not
    /// supported, this can be dangerous if this function is called many times
    /// in an application because the memory leak will overwhelm the page
    /// quickly and crash the wasm.
    pub fn into_js_value(self) -> JsValue {
        let idx = self.js.idx;
        mem::forget(self);
        JsValue::_new(idx)
    }

    /// Same as `mem::forget(self)`.
    ///
    /// This can be used to fully relinquish closure ownership to the JS.
    pub fn forget(self) {
        mem::forget(self);
    }

    /// Create a `Closure` from a function that can only be called once.
    ///
    /// Since we have no way of enforcing that JS cannot attempt to call this
    /// `FnOne(A...) -> R` more than once, this produces a `Closure<dyn FnMut(A...)
    /// -> R>` that will dynamically throw a JavaScript error if called more
    /// than once.
    ///
    /// # Example
    ///
    /// ```rust,no_run
    /// use wasm_bindgen::prelude::*;
    ///
    /// // Create an non-`Copy`, owned `String`.
    /// let mut s = String::from("Hello");
    ///
    /// // Close over `s`. Since `f` returns `s`, it is `FnOnce` and can only be
    /// // called once. If it was called a second time, it wouldn't have any `s`
    /// // to work with anymore!
    /// let f = move || {
    ///     s += ", World!";
    ///     s
    /// };
    ///
    /// // Create a `Closure` from `f`. Note that the `Closure`'s type parameter
    /// // is `FnMut`, even though `f` is `FnOnce`.
    /// let closure: Closure<dyn FnMut() -> String> = Closure::once(f);
    /// ```
    ///
    /// Note: the `A` and `R` type parameters are here just for backward compat
    /// and will be removed in the future.
    pub fn once<F, A, R>(fn_once: F) -> Self
    where
        F: WasmClosureFnOnce<T, A, R>,
    {
        Closure::wrap(fn_once.into_fn_mut())
    }

    /// Convert a `FnOnce(A...) -> R` into a JavaScript `Function` object.
    ///
    /// If the JavaScript function is invoked more than once, it will throw an
    /// exception.
    ///
    /// Unlike `Closure::once`, this does *not* return a `Closure` that can be
    /// dropped before the function is invoked to deallocate the closure. The
    /// only way the `FnOnce` is deallocated is by calling the JavaScript
    /// function. If the JavaScript function is never called then the `FnOnce`
    /// and everything it closes over will leak.
    ///
    /// ```rust,ignore
    /// use wasm_bindgen::{prelude::*, JsCast};
    ///
    /// let f = Closure::once_into_js(move || {
    ///     // ...
    /// });
    ///
    /// assert!(f.is_instance_of::<js_sys::Function>());
    /// ```
    ///
    /// Note: the `A` and `R` type parameters are here just for backward compat
    /// and will be removed in the future.
    pub fn once_into_js<F, A, R>(fn_once: F) -> JsValue
    where
        F: WasmClosureFnOnce<T, A, R>,
    {
        fn_once.into_js_function()
    }
}

/// A trait for converting an `FnOnce(A...) -> R` into a `FnMut(A...) -> R` that
/// will throw if ever called more than once.
#[doc(hidden)]
pub trait WasmClosureFnOnce<FnMut: ?Sized, A, R>: 'static {
    fn into_fn_mut(self) -> Box<FnMut>;

    fn into_js_function(self) -> JsValue;
}

impl<T: ?Sized> AsRef<JsValue> for Closure<T> {
    fn as_ref(&self) -> &JsValue {
        &self.js
    }
}

/// Internal representation of the actual owned closure which we send to the JS
/// in the constructor to convert it into a JavaScript value.
#[repr(transparent)]
struct OwnedClosure<T: ?Sized>(Box<T>);

unsafe extern "C" fn destroy<T: ?Sized>(a: usize, b: usize) {
    // This can be called by the JS glue in erroneous situations
    // such as when the closure has already been destroyed. If
    // that's the case let's not make things worse by
    // segfaulting and/or asserting, so just ignore null
    // pointers.
    if a == 0 {
        return;
    }
    drop(mem::transmute_copy::<_, Box<T>>(&(a, b)));
}

impl<T> WasmDescribe for OwnedClosure<T>
where
    T: WasmClosure + ?Sized,
{
    #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
    fn describe() {
        inform(CLOSURE);
        inform(destroy::<T> as *const () as usize as u32);
        inform(T::IS_MUT as u32);
        T::describe();
    }
}

impl<T> IntoWasmAbi for OwnedClosure<T>
where
    T: WasmClosure + ?Sized,
{
    type Abi = WasmSlice;

    fn into_abi(self) -> WasmSlice {
        let (a, b): (usize, usize) = unsafe { mem::transmute_copy(&ManuallyDrop::new(self)) };
        WasmSlice {
            ptr: a as u32,
            len: b as u32,
        }
    }
}

impl<T> WasmDescribe for Closure<T>
where
    T: WasmClosure + ?Sized,
{
    #[cfg_attr(wasm_bindgen_unstable_test_coverage, coverage(off))]
    fn describe() {
        inform(EXTERNREF);
    }
}

// `Closure` can only be passed by reference to imports.
impl<T> IntoWasmAbi for &Closure<T>
where
    T: WasmClosure + ?Sized,
{
    type Abi = u32;

    fn into_abi(self) -> u32 {
        (&*self.js).into_abi()
    }
}

impl<T> OptionIntoWasmAbi for &Closure<T>
where
    T: WasmClosure + ?Sized,
{
    fn none() -> Self::Abi {
        0
    }
}

fn _check() {
    fn _assert<T: IntoWasmAbi>() {}
    _assert::<&Closure<dyn Fn()>>();
    _assert::<&Closure<dyn Fn(String)>>();
    _assert::<&Closure<dyn Fn() -> String>>();
    _assert::<&Closure<dyn FnMut()>>();
    _assert::<&Closure<dyn FnMut(String)>>();
    _assert::<&Closure<dyn FnMut() -> String>>();
}

impl<T> fmt::Debug for Closure<T>
where
    T: ?Sized,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Closure {{ ... }}")
    }
}

impl<T> Drop for Closure<T>
where
    T: ?Sized,
{
    fn drop(&mut self) {
        // Decrease refcount on the JS side, this will automatically free
        // the Rust data if we're the last owner.
        self.js._wbg_cb_unref();
    }
}

/// An internal trait for the `Closure` type.
///
/// This trait is not stable and it's not recommended to use this in bounds or
/// implement yourself.
#[doc(hidden)]
pub unsafe trait WasmClosure: WasmDescribe {
    const IS_MUT: bool;
}

/// An internal trait for the `Closure` type.
///
/// This trait is not stable and it's not recommended to use this in bounds or
/// implement yourself.
#[doc(hidden)]
pub trait IntoWasmClosure<T: ?Sized> {
    fn unsize(self: Box<Self>) -> Box<T>;
}
