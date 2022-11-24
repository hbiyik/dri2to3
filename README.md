# dri2to3

This is a library implementing DRI2 on top of DRI3 for running Mali
blob X11 drivers under Xwayland. (Note that the Wayland blob may not
expose all of the required extensions for Xwayland.)

## Compiling

```text
mkdir build
cd build
meson setup
ninja
```

## Usage

`LD_PRELOAD=/path/to/dri2to3/build/libdri2to3.so LD_LIBRARY_PATH=/path/to/libmali/x11 es2gears_x11`

With gl4es, instead use this for `LD_LIBRARY_PATH`:

`LD_LIBRARY_PATH=/path/to/gl4es/lib:/path/to/libmali/x11`

To get the blob driver:

```text
mkdir -p ~/libmali/x11
cd ~/libmali/x11
wget https://github.com/JeffyCN/rockchip_mirrors/raw/libmali/lib/aarch64-linux-gnu/libmali-valhall-g610-g6p0-x11-gbm.so
ln -s libmali-valhall-g610-g6p0-x11-gbm.so libmali.so.1
for l in libEGL.so libEGL.so.1 libgbm.so.1 libGLESv2.so libGLESv2.so.2 libOpenCL.so.1; do ln -s libmali.so.1 $l; done
```
