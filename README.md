going
=====

Ensure your processes are still `going`. A simple ICS licensed
process supervisor.


Installation
------------

To install `going` to `/usr/sbin` simply invoke:

    make install

The default `/usr` prefix can be changed with:

    PREFIX=/usr/local make install


Usage
-----

Add simple configs for starting your services to `/etc/going.d`:

    cat >>/etc/going.d/salt-minion <<EOF
    cmd=/usr/bin/salt-minion
    EOF

Add `going` to `/etc/inittab` with the `respawn` action:

    GO:12345:respawn:/usr/sbin/going

Make `init(8)` reload its configuration:

    init q

You can gracefully reload configurations after you've added or
removed files in `/etc/going.d` with:

    kill -HUP <pid of going>

Reloading configurations for already running processes is currently not
supported without sending `going` a `TERM` signal which terminates all
its supervised processes.


Goal
----

* No more than 1024kB VmPeak.
* No configuration of `going`.
* Simple configuration files for services.
* Under 500 actual LOC determined by `cloc`.


Alternatives
------------

### Process runners

* System V init's [`/etc/inittab`](http://man.cx/inittab(4)
* BSD init's [`/etc/ttys`](http://www.freebsd.org/cgi/man.cgi?query=ttys)
* [daemontools](http://cr.yp.to/daemontools.html)
* [runit](http://smarden.org/runit/)
* [s6](http://www.skarnet.org/software/s6/index.html)
* [perp](http://b0llix.net/perp/)
* [supervisord](http://supervisord.org/)
* [Upstart](http://upstart.ubuntu.com/)
* [systemd](http://www.freedesktop.org/wiki/Software/systemd/)
* [mon](https://github.com/visionmedia/mon)

### Poll based monitors

* [restartd](http://packages.debian.org/unstable/restartd)
* [monit](http://mmonit.com/monit/)
* [god](http://godrb.com/)


TODO
----

### 1.0.0

* Fix all inline TODOs in source.
* Fix memory leak when adding new child.
* Check if we have memory leak when removing child.
* Look into starting services with fresh environment, processgroup,
  stdinn/out/err.
* Document requirements (kernel version) with note about usage of non-portable
  system calls.
* Literate programming:
  - Look into using solarized color scheme for pygments.
  - Link references to library functions in section 3 to online man pages
    on kernel.org.
  - Link to `going` man pages in introduction.
* Man pages for daemon: going(1) and config format: going(5) using
  [ronn][].
  - Hook generation up to `make doc` target.
* Look into [foreman][] format.
* Use [semantic versioning][semantic].
* Tag releases in git.

### 1.1.0

* Add support for setting the environment.
* Add support for `uid` and `gid`.
* Possibly add support for `chdir`.
* Possibly add support for `chroot`.
* Possibly create backoff algorithm for quarantined children in stead of
  quarantining them a constant time.
* Possibly use higher resolution timers for childrens uptime with
  `clock_gettime(CLOCK_MONOTONIC)`.

### 2.0.0

* Use `inotify` to detect new configs (and stop services in remove configs).
  - Should make SIGHUP reloading obselete.
  - Need to change handling of SIGCHLD to use [pselect(2)][pselect],
    [ppoll(2)][ppoll], [epoll_pwait(2)][epoll] or [signalfd(2)][signalfd] to
    avoid [signal races][race]. Of all the pollers ppoll could be the most
    efficient for a small set of file descriptors but signalfd paired with
    a normal select, poll, or epoll could be the cleanest implementation
    requiring no signal handler and global flag set from it. Also
    see *Combining Signal and Data Events* in [select_tut(2)][select_tut]
    for more info of the signal safe I/O multiplexers.

### 2.1.0

* Create going information utility:
  - Uptime of children.
  - More statistics either from /proc or getrusage().

### Other

* Possibly logging stdin/sterr with custom log per service.
  - Need to switch handling of SIGCHLD as for `inotify`. If ppoll is used
    for handling inotify we should probably shift to epoll_pwait.
* Asynchronous starting of processes.
* Look into using `scan-build` in debug make target.


[shocco]: http://rtomayko.github.com/shocco/
[ronn]: https://github.com/rtomayko/ronn
[foreman]: http://ddollar.github.com/foreman/
[semantic]: http://semver.org/
[pselect]: http://www.kernel.org/doc/man-pages/online/pages/man2/select.2.html
[ppoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/poll.2.html
[epoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/epoll_wait.2.html
[signalfd]: http://www.kernel.org/doc/man-pages/online/pages/man2/signalfd.2.html
[race]: http://www.linuxprogrammingblog.com/code-examples/using-pselect-to-avoid-a-signal-race
[select_tut]: http://www.kernel.org/doc/man-pages/online/pages/man2/select_tut.2.html
