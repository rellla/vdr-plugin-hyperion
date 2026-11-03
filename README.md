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

Hyperion is controlled with its JSON server (default port 19444, short connections only when
something changes):

- The ambient light can be switched off and on with the main menu entry, a key macro (e.g.
  "Green @hyperion" in keymacros.conf) or with SVDRP (PLUG hyperion AMBI). Depending on
  hyperion.OffAction, the led device of Hyperion is switched off or only the video isn't sent
  anymore. The state isn't stored, after a restart of VDR the ambient light is on.
- With hyperion.LedsFollowAttach = 1, the led device is switched off with a detach of the output
  device and when VDR ends (e.g. the box is switched off), and on again with an attach or the next
  start.
- Without video (radio), the leds can show an effect or stay dark (hyperion.IdleMode).
- While the playback is paused, the leds can be dimmed (hyperion.PauseBrightness). They are dimmed
  after half a second (not, if single frames are stepped) and fade within 1.5 seconds. With the
  playback, the full brightness is restored at once.
- Without hyperion.LedsFollowAttach, the plugin doesn't switch the led device on at the start: a led
  device, which was switched off elsewhere (e.g. in the web interface), stays off.
- The setup page shows the state of the ambient light, the output plugin, the connection, the
  format of the frames with the time to read an image from a frame and the last error.
- Like the video, the JSON commands apply to all running instances of Hyperion. Without a token,
  Hyperion only accepts them from the local network (and if "Local API authentication" is
  disabled, the default).

Search Hyperion
---------------
"Search Hyperion..." on the setup page lists the Hyperion servers in the local network (SSDP,
Hyperion always answers it). The selected server is put into host and ports of the setup page,
which still has to be stored with OK. If the host changes, the token is cleared: a token only
belongs to the Hyperion, which created it. If the flatbuffers server of a Hyperion is disabled,
it has to be enabled in its web interface (Network services).

Test pattern
------------
"Test pattern..." on the setup page shows a pattern on the whole screen (OSD) and sends the same
pattern to Hyperion at the same moment (instead of the video):

- Colors: red, green, blue and white, every 2 seconds. Shows, if the color order of the leds is
  right.
- Edges: top red, right green, bottom blue, left yellow. Shows, if the layout of the leds (start,
  direction, corners) is right.
- Delay: black and white, every second. The leds should change together with the screen. If they
  are ahead (a TV with a slow picture processing), Left/Right changes the delay of the leds, it is
  written into Hyperion at once. The delay needs the smoothing of Hyperion. With the smoothing,
  the leds fade within the smoothing time: compare the start of the change.

Up/Down switches the pattern, Back or OK returns to the menu and the video. The pattern is shown,
even if the ambient light is switched off.

Settings in Hyperion
--------------------
The setup page opens "Settings in Hyperion...", where some settings of Hyperion itself can be
changed:

- smoothing: on/off, the time to reach a new color and a delay of the leds (in frames of the
  smoothing, e.g. 40 ms at 25 Hz; it helps, if the leds are ahead of a TV with a slow picture
  processing, see the test pattern "Delay"; Hyperion delays the leds only with the smoothing)
- black border detection: on/off, the threshold (max. brightness of a black border) and the mode
- brightness of the leds

The values are read from Hyperion, when the menu is opened, and written with OK. They are stored
in the configuration of Hyperion (not in setup.conf), so the web interface shows the same values.
Only the changed values are written, into every instance of Hyperion (all of them get the video):
the other values of an instance stay as they are. The menu shows the values of the first enabled
instance. The brightness is written into every color adjustment of an instance.

Hyperion 2.1 or newer is needed. Hyperion only allows to read and write its configuration after
a login with admin rights:

- As long as Hyperion has its default password, the plugin logs in with it, as the web interface
  does (which doesn't ask for a password then). Nothing has to be set up.
- If a password was set in Hyperion, a token is needed (hyperion.Token). The easiest way to get
  one is "Request token..." on the setup page: the plugin sends a token request with a code of 5
  characters, which is shown in the setup. Hyperion shows the request with the same code in its
  web interface (the next time it is opened, if it isn't open). When it is accepted there (within
  3 minutes), the plugin receives the token and stores it at once. OK on the item cancels the
  request, leaving the setup page as well. A token can also be created in the web interface under
  Network services, Token management and entered on the setup page.

The password is never guessed: Hyperion blocks the login for 10 minutes after 10 failed attempts.

Follow the TV (CEC)
-------------------
The ambient light can follow the TV with the plugin
[cecremote](https://www.uli-eckhardt.de/vdr/cec.de.shtml): it runs SVDRP AMBI, when the TV
sends a CEC command. Add to cecremote.xml (in `<config>`):

	<!-- TV (logical address 0) goes to standby: ambient light off -->
	<onceccommand command="STANDBY" initiator="0">
		<commandlist>
			<exec>svdrpsend PLUG hyperion AMBI off</exec>
		</commandlist>
	</onceccommand>
	<!-- TV is switched on: ambient light on -->
	<onceccommand command="REQUEST_ACTIVE_SOURCE" initiator="0">
		<commandlist>
			<exec>svdrpsend PLUG hyperion AMBI on</exec>
		</commandlist>
	</onceccommand>
	<onceccommand command="SET_STREAM_PATH" initiator="0">
		<commandlist>
			<exec>svdrpsend PLUG hyperion AMBI on</exec>
		</commandlist>
	</onceccommand>

- Every TV sends standby, but the commands after switching on differ: most TVs send
  REQUEST_ACTIVE_SOURCE or SET_STREAM_PATH, some ROUTING_CHANGE. The commands of the TV are logged
  with `<cecdebug>` in cecremote.xml.
- With hyperion.OffAction = 0, the led device of Hyperion is switched off and on, so the leds are
  dark also without video. With 1, only the video isn't sent anymore.
- The plugin itself doesn't use CEC, so it doesn't compete with cecremote (or Kodi) for the CEC
  adapter. If Hyperion runs on its own device, which is connected to the TV with HDMI, Hyperion can
  also handle the CEC commands itself (CEC events in its web interface).

Requirements
------------
- VDR 2.6.0 or newer
- nlohmann/json 3.7 or newer (header only, e.g. Debian/Ubuntu: nlohmann-json3-dev)
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

	hyperion.JsonPort = 19444
		port of the Hyperion JSON server (used to control Hyperion)

	hyperion.Token =
		authorization token of the Hyperion JSON server (empty = none).
		Needed for the settings in Hyperion, if a password was set in
		Hyperion, for a Hyperion outside of the local network or with
		"Local API authentication" enabled in Hyperion.

	hyperion.OffAction = 0
		what switching the ambient light off (main menu, key macro,
		SVDRP AMBI) does
		0 = switch the led device of Hyperion off
		1 = only stop sending the video (Hyperion shows the lower priorities)

	hyperion.LedsFollowAttach = 0
		0 = the led device isn't switched with the attach state
		1 = switch the led device on with an attach (and at the start),
		    off with a detach (and when VDR ends)

	hyperion.IdleMode = 0
		what is shown, if no video is displayed for two seconds (e.g. radio)
		0 = give the leds back (Hyperion shows the lower priorities)
		1 = the effect hyperion.IdleEffect
		2 = dark leds

	hyperion.IdleEffect = Rainbow swirl
		name of the Hyperion effect for hyperion.IdleMode = 1

	hyperion.PauseBrightness = 100
		0 - 100 = brightness of the ambient light while the playback is
		paused (%). 100 = not dimmed.

	hyperion.HideMainMenuEntry = 0
		1 = hide the main menu entry, which switches the ambient light
		    on and off

SVDRP commands
--------------

	AMBI [on | off | toggle]
		Switch the ambient light on or off (not stored).
		Without an argument the ambient light state is returned.
		Reply code 550: Hyperion is disabled in the setup.

	STAT
		Show the state of the ambient light as "key: value" lines, e.g.
		for home automation:
		  state: video          disabled, no_output, detached, off, video,
		                        paused, kept, released, effect or dark
		  on: 1                 0 = switched off by the user (AMBI off)
		  output: drmdevice     output plugin, which provides the video
		  server: 127.0.0.1:19400
		  connected: 1          connected to the flatbuffers server
		  priority: 150
		  leds: on              led device set by the plugin (on, off, -)
		  images_per_second: 25
		  format: NV12/SAND128 1920x1080
		  image: 80x45          size of the sent image
		  sample_us: 210        average/max. time to read an image from
		  sample_us_max: 480    a frame (microseconds)
		  error:                last error (empty: none)
		  error_time:           time of the last error (seconds since 1970)

Unit tests
----------
The tests use Catch2 v3: make test
