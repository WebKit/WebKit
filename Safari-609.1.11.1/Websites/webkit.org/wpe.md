WPE is the reference WebKit port for embedded and low-consumption computer devices. It has been designed from the ground-up with performance, small footprint, accelerated content rendering, and simplicity of deployment in mind, bringing the excellence of the WebKit engine to countless platforms and target devices.

In times where embedded devices power endless ad-hoc applications in a diverse range of industries and the Web continues to establish itself as one of the most popular application development frameworks for developers, WPE closes this gap and serves as a foundation over which embedders can build rich applications that run entirely on the Web.

## Design goals

The design goals that set WPE apart from other ports of WebKit and other engines are:

* **To provide a no-frills, straight to the point, web runtime   for embedded devices.**

  Thanks to the primary focus on the embedded world, WPE is being developed each step of the way with the needs and constraints of embedded devices in consideration, allowing for the best experience in all kind of web-based applications.

* **To be fast and lightweight, keeping software dependencies to a minimum.**

  The minimal set of dependencies needed to run WPE ensures that its footprint is small and that applications built with WPE can run in low-end devices.

* **To keep up with Web standards and to continuously work in ensuring compliance.**

  By its complete upstream integration, WPE can leverage the work on Web standards that goes into the WebKit project. Additionally, the WPE team is committed to ensure that new specifications are implemented in WPE with our goals in mind.

* **To use hardware acceleration wherever its advantageous: WebGL, accelerated canvas, CSS 3D transforms, video playback.**

  For best performance, responsiveness, and user-experience, WPE enables deployments to make the best out of hardware acceleration capabilities present in the target embedded devices.

* **To make deployment in new platforms and target devices as easy as possible, through a backend architecture.**

  WPE has been designed with a backend architecture, which allows easily developing backends for the widest range of platform of devices, including, for example, Wayland and Raspberry Pi devices.

## A multimedia-oriented web engine

Because of the extensive growth of multimedia in the embedded sector, WPE has a strong focus on multimedia applications. Some of the key multimedia features in WPE are:

* Hardware-accelerated video rendering and CSS transformations.
* Punch-hole video playback available when needed by the target platform.
* MSE (MP4, WebM, VP9, Opus) supported and under constant development, optimized for YouTube and YouTube TV.
* EME (V1 and V3, Clearkey, other third-party DRM frameworks) supported and constantly improving.
* GStreamer-based multimedia framework.
* WebRTC partially supported and under heavy development.

## How to get WPE?

WPE is an upstream WebKit port, which means that you can just [get it the usual way](https://webkit.org/getting-the-code/). You can build the code following the instructions in the [WPE Wiki page](https://trac.webkit.org/wiki/WPE).

The WPE team is making periodic releases and these are available at <https://wpewebkit.org/>.

## How to contribute?

The WPE port has a mailing list where development and general questions can be addressed. You can subscribe at [https://lists.webkit.org/mailman/listinfo/webkit-wpe](https://lists.webkit.org/mailman/listinfo/webkit-wpe).

You can report issues, bugs, and file feature requests in WebKit Bugzilla, under the WebKit WPE component: [http://bugs.webkit.org](http://bugs.webkit.org/). You can follow the instructions in [https://webkit.org/reporting-bugs/](https://webkit.org/reporting-bugs/).

If you want to submit code, you can follow the instructions at [https://webkit.org/getting-started/](https://webkit.org/getting-started/).
