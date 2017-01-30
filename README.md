# what
rfsend is simple 433mHz transmitter driver for Linux working over single gpio pin. It works with $1 devices like this one:

![](http://i01.i.aliimg.com/wsphoto/v0/32303285841/15mA-315MHZ-DC3-12V-Single-Shot-Wireless-Send-font-b-Module-b-font-font-b-FS1000A.jpg)

# usage

```
$ sudo insmod rfsend tx_pin=<pin>

$ echo h500f1000_hhhhhhhhhhhffhffhhhhhhfffhhhhffhhhhhhhhhhhhhhhhffhhffhhff > /dev/rfsend
```

First goes map of level durations, a letter (a-z) is followed by delay in µS.

Then `_` means to start from low level, `^` would start from high.

After that each letter will wait for its assigned duration and switch the level to opposite thus allowing to modulate signal.

The transmission is unconditionally prepended by interleave of low and high levels for 50-100mS each to allow receiver to tune AGC.

Only one packet per write is supported that must be a single line in the above format.

# udev

Put this to /etc/udev/rules.d/98-rfsend.rules to tune permissions on /dev/rfsend
```
KERNEL=="rfsend", SUBSYSTEM=="rfsend", MODE="0666"
```
