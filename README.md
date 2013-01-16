# SimpleLinuxRamDisk

---

## License

This program is licensed under the GPL. For detailed information see `LICENSE` file.

## tmpfs

Many Unix distributions have already implemnte a RAM disk filesystem, especially *tmpfs* is supported since kernel version 2.4 and up. With the command `df` you can show which special file uses *tmpfs*
Example output of `df`:

<code><pre>
Filesystem     1K-blocks     Used Available Use% Mounted on
/dev/sda1       23738812 15785016   6747948  71% /
udev             1016456        4   1016452   1% /dev
tmpfs             410264      740    409524   1% /run
none                5120        0      5120   0% /run/lock
none             1025652      684   1024968   1% /run/shm
none              102400       12    102388   1% /run/user
</pre></code>

There you can see that `/run` has *tmpfs* as filesystem. `/run` can be used as RAM drive directly. Moreover you could create a new RAM disk with the following command: `sudo mount -t tmpfs -o size=20% none /path_to_mount_position`

* parameter *size* is optinal and can be used with **%** or **k, m, g** for **K**ilo-, **M**ega- and **G**igabyte.
* without *size* parameter, the half of your physical RAM is used
* the RAM disk can be resized on-the-fly with this command: `mount -o remount ...`
* because we doesn't mount a device, the parameter is *none*
* the rest should be self-explanatory
* example `sudo mount -t tmpfs -o size=128m none /media/my_ram_disk`
* for more documentation [read this](http://www.kernel.org/doc/Documentation/filesystems/tmpfs.txt)

---

Andreas Linz /// klingt.net(at)googlemail.com /// 2012