<p align="center"><img src="doc/img/realsense.png" width="70%" /><br><br></p>





## Overview
Intel® RealSense™ SDK is a cross-platform library for Intel® RealSense™ depth cameras. This special 2.56.93 ED version enables the D438 prototype cameras which outputs MJPEG image format from the new 13M RGB sensor.

In addition to D438 features, this version of SDK also supports same common capabilities as the regular SDK. It allows depth and color streaming, and provides intrinsic and extrinsic calibration information.
The library also offers synthetic streams (pointcloud, depth aligned to color and vise-versa).

For details of common SDK capabilties and usages, please refer to https://github.com/IntelRealSense/librealsense/blob/master/readme.md

## What’s included in the SDK:
| What | Description | Download link|
| ------- | ------- | ------- |
| **[Intel® RealSense™ Viewer](./tools/realsense-viewer)** | With this application, you can quickly access your Intel® RealSense™ Depth Camera to view the depth stream, visualize point clouds, record and playback streams, configure your camera settings, modify advanced controls, enable depth visualization and post processing  and much more. | [**realsense-viewer.exe**](https://github.com/gwen2018/librealsense/tree/d438/download) |
| **[Depth Quality Tool](./tools/depth-quality)** | This application allows you to test the camera’s depth quality, including: standard deviation from plane fit, normalized RMS – the subpixel accuracy, distance accuracy and fill rate. You should be able to easily get and interpret several of the depth quality metrics and record and save the data for offline analysis. |[**rs-depth-quality.exe**](https://github.com/gwen2018/librealsense/tree/d438/download) |
| **[Debug Tools](./tools/)** | Device enumeration, FW logger, etc as can be seen at the tools directory | [**Debug Tools**](https://github.com/gwen2018/librealsense/tree/d438/download)|
| **[Code Samples](./examples)** |These simple examples demonstrate how to easily use the SDK to include code snippets that access the camera into your applications. Check some of the [**C++ examples**](./examples) including capture, pointcloud and more and basic [**C examples**](./examples/C) | [**Samples**](https://github.com/gwen2018/librealsense/tree/d438/download) |


## Example Streaming with D438
Use Realsense Viewer to stream 848x480 Depth and 4160x3120 RGB8 format at 15 fps, for example,
https://github.com/gwen2018/librealsense/tree/d438/download/d438-streaming-example.png

1) RGB sensor hardware output MJPEG format, SDK decode and convert into RGB8 for processing and rendering. RGB8 is also the output to ROS for publishing.
2) MJPEG format in viewer streaming only, no rendering, so black screen but can show meta data including frame rates.
3) RAW format in viewer streaming only, no rendering, so black screen but can show meta data including frame rates.
4) Full list of supported formats [**D438 supported formats**](https://github.com/gwen2018/librealsense/tree/d438/download/d438-supported-formats.txt)


## Source Repository
```
git clone -b d438 https://github.com/gwen2018/librealsense.git 
```

## Supported Platforms
This ED was tested on the following platforms:
  1) Intel NUC with Ubuntu 22.04.1 LTS and ROS2 humble
  2) Nvidia Jetson Xavier with Jetpack 5.0.2 and Nvidia Jetson Orin with Jetpack 5.1.2 
  3) Intel NUC with Windows 10 and Windows 11


## Building on Linux

   1) Install nasm, this is required to compile sdk for d438 since it uses libjpegturbo for JPEG image decoding.
   
```
sudo apt-get install nasm
```
 
   2) Checkout sources and compile, please note the new BUILD_WITH_JPEG_TURBO=true option
```
      cd ~ && \
      git clone -b d438 https://github.com/gwen2018/librealsense.git && \
      cd librealsense && \
      sudo ./scripts/setup_udev_rules.sh && \
      mkdir build && \
      cd build/ && \
      cmake ../ \
      -DBUILD_SHARED_LIBS=true \
      -DBUILD_WITH_JPEG_TURBO=true \
      -DBUILDPYTHON_BINDINGS:bool=true \
      -DBUILD_WITH_CUDA=false \
      -DFORCE_RSUSB_BACKEND=false \
      -DPYTHON_EXECUTABLE=/usr/bin/python3 \ -DMAKE_BUILD_TYPE=Release && \
      make -j$(cat /proc/cpuinfo |grep proc |wc -l) && \
      sudo make install && \
      sudo mv libjpeg-turbo/lib/libturbojpeg.so* /usr/local/lib
```
 
  3) Check the realsense and libjpeg turbo libraries are copied properly under /usr/local/lib
 ```
     ls /usr/local/lib

     librealsense2-gl.so -> librealsense2-gl.so.2.56
     librealsense2-gl.so.2.56 -> librealsense2-gl.so.2.56.92
     librealsense2-gl.so.2.56.0
     librealsense2-gl.so.2.56.92
     librealsense2.so -> librealsense2.so.2.56
     librealsense2.so.2.56 -> librealsense2.so.2.56.92
     librealsense2.so.2.56.0
     librealsense2.so.2.56.92
     libturbojpeg.so -> libturbojpeg.so.0
     libturbojpeg.so.0 -> libturbojpeg.so.0.3.0
     libturbojpeg.so.0.3.0
 ```

  4) If /usr/local/lib is not already in LD_LIBRARY_PATH, make sure it is.
```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
```

  6) Launch realsense-viewer, the cameras should be identified as D438 in the viewer.
```
realsense-viewer
```

  8) The RGB sensor is 13M, so the maximum resolution is 4160x3120@15 fps, choose RGB8 format in the viewer for rendering the RGB image graphically. the camera itself output JPEG but the SDK converts in into RGB8.

## Building on Nvidia Jetson Xavier and Orin
Similar as compilation on Intel Ubuntu platform above. To utilize CUDA capabilities on the Jetson platform, enable CUDA while compile with -DBUILD_WITH_CUDA=true

## Building on Windows
1) Download nasm compiler ZIP, unzip it and make sure the executable in PATH.
   https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/win64/nasm-2.15.05-win64.zip
2) Use cmake and Visual Studio 2019 to compile


## ROS support
   D438 supports ROS2 with a special version of Realsense ROS Wrapper.

```
      git clone -b d438 https://github.com/gwen2018/realsense-ros.git
```

   a) compile the Realsense ROS2 wrapper
```
      cd realsense-ros
      source /opt/ros/humble/setup.bash
      colcon build
      source install/setup.bash
```

   b) connect D438 camera and launch camera node with rs_launch.py sample script
```
      ros2 launch realsense2_camera rs_launch.py depth_module.profile:=848x480x15 rgb_camera.profile:=4160x3120x15 diagnostics_period:=1.0
```      


## License
This project is licensed under the [Apache License, Version 2.0](LICENSE).
Copyright 2024 Intel Corporation
