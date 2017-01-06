# Bubble Streaming Client

A client for the propriety `bubble` streaming prococol which is included in the line of security CCTV IP cameras DVR cameras using HiSilicon chipsets (Hi3510, Hi3516, Hi3518 etc.). This can be used to obtain and decode the video streams from the cameras for any further processing and application as the many vendors disable standard streaming protocol such as RTMP.

The protocol is reverse engineered through packets capturing and from a non-official version of the firmware's codes located at [zackxue/ipc](https://github.com/zackxue/ipc) and so may not be exhaustive or require modifications if a particular vendor makes any changes.

## Dependencies

* *C++ Boost 1.63.0*
* *FFMPEG 3.2.2*
* *OpenCV 3* (This is used for an example processing application and can be optionally removed by removing the `Processor` class and OpenCV compilation flags in `Makefile`)

## Compilation

Change or add paths to the required libraries in `Makefile` if necessary. In the source code directory, run:

> make

The output binary `bubble_client` is located in `./build` by default

## Running

In the source code directory, run:

> ./build/bubble\_client server\_ip server\_port username password

*server_port* is typically 80.
