
Download the latest release (in Dashlaunch plugin format) from:
    https://github.com/InvoxiPlayGames/UsbdSecPatch/releases

==========================================================================

This is a quick and dirty Xbox 360 kernel patch to do 2 things, that
should make using a custom controller much easier.

- Patches UsbdIsDeviceAuthenticated to always report true -
  this will allow any device that doesn't complete authentication to
  behave as any of the protected device classes (e.g. controller)

- Patches WgcAddDevice to skip over a check for a certain interface,
  that is usually missing on certain custom controller firmwares.

This has been tested to work with a Pi Pico running the "Ardwiino"
firmware version 8.9.4, set as an XInput Guitar Hero controller.

This fork additionally fixes identical clones colliding as one controller:

- Hooks XamUserBindDeviceCallback; when a new binding's controllerId is
  already live under a different device context, the new binding gets a
  unique bumped id (and disconnects are translated back by context).
  This makes two+ identical XInput clones (same VID/PID, no usable serial
  identity) register as separate controllers. Verified with two 8BitDo
  Ultimate 2C units (2dc8:310a): two ROL quadrants, independent input,
  working in-game on retail 17559.
- To host the hook the plugin now stays resident (the self-unload trick
  is removed).

No guarantees are made to the stability or effectiveness of this patch.
Please note that you have to re-plug your controllers *after* power-on.

(Source for a XeBuild patch is also provided. This has NOT been tested.)
