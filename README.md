vdr-plugin-hyperion
===================

A plugin for the Video Disk Recorder (VDR), which sends the displayed video to
[Hyperion](https://github.com/hyperion-project/hyperion.ng) (ambient lighting).

Written by Andreas Baierl <ichgeh@imkreisrum.de>.

License: AGPL-3.0-or-later (see LICENSE.md).

How does it work?
-----------------
The plugin doesn't grab the screen itself. It gets a small image of the displayed video from the
output plugin with the VDR service "AmbientImage-v1.0" (see ambientimage_service.h). It is provided
by [drmdevice](https://github.com/rellla/vdr-plugin-drmdevice). Another output plugin may provide
the same service.

- The service call waits for the next frame of the video. The output plugin reads only a few
  pixels per image pixel directly from the decoded frame, in the thread of this plugin. Its display
  thread only hands over a snapshot of the displayed buffer.
- HDR frames are tone mapped to SDR by the output plugin.
- Only the video is sent (no OSD). A scaled video or a video with a different aspect ratio is placed
  into the image as it is shown on the screen, the rest is black.

The image is sent to the flatbuffers server of Hyperion (default port 19400, it has to be enabled
in Hyperion under Network services). The plugin registers as "VDR" with the configured priority.
No grabber is needed in Hyperion.

- While the video is paused, the last image is repeated once per second, so that Hyperion keeps it.
- If no video is shown for two seconds (e.g. radio, but not with every channel switch) or the output
  device is detached, the priority is cleared and the connection is closed, so the leds fall back
  to the lower priorities (e.g. an effect).
- If Hyperion isn't reachable, the plugin retries in the background with an increasing delay.

Requirements
------------
- VDR 2.6.0 or newer
- an output plugin, which provides the service AmbientImage-v1.0 (drmdevice)
- Hyperion.ng with the flatbuffers server enabled

Installation
------------
Build it like any other VDR plugin and load it with -P hyperion.

Setup: /etc/vdr/setup.conf
--------------------------

	hyperion.Enable = 1
		0 = disabled
		1 = send the video to Hyperion

	hyperion.Host = 127.0.0.1
		host name or address of the Hyperion server

	hyperion.Port = 19400
		port of the Hyperion flatbuffers server

	hyperion.Priority = 150
		100 - 199 = priority of the input in Hyperion (lower values win)

	hyperion.Fps = 25
		1 - 50 = max. images per second sent to Hyperion

	hyperion.Width = 80
		16 - 320 = width of the image sent to Hyperion (the height
		follows the aspect ratio of the screen)

Unit tests
----------
The tests use Catch2 v3: make test
