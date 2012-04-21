going
=====

Ensure your processes are still `going`. A simple ICS licensed
process supervisor.


Installation
------------

To install `going` to `/usr/sbin` simply invoke:

    make install

The default `/usr` prefix can be changed with:

    make install prefix=/usr/local


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
its supervised processeses.

All abnormal events will be logged to the daemon syslog facility which
normally can be inspected in `/var/log/daemon.log`.


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

* Look into starting services with fresh environment, processgroup,
  stdinn/out/err.
* Literate programming:
  - Connect the paragraphs so that it reads well.
  - Look into using solarized color scheme for pygments.
  - Link references to library functions in section 3 to online man pages
    on kernel.org.
  - Link to `going` man pages in introduction.
* Man pages: 
  - Daemon: going(1).
  - Config format: going(5).
* GH-pages:
  - Create index.html with:
    - Header.
    - Tagline.
    - Navigation:
      - going(1), going(5) (link to html ronn output).
      - Documentation (link to literate source).
      - Collabortion (link to GH repo).
      - LICENSE.
    - Download section:
      - Links to tarball of release tags served by GH.
      - Link into CHANGES document.
      - Checksum.
  - Possibly append analytics tracking with Makefile.
  - Remove content from README and point to GH-page.
* Look into [foreman][] format.
* Use [semantic versioning][semantic].
* Tag releases in git.
* Create Arch Linux package.
* Create Debian and RPM package (possibly using RPM).

### 1.1.0

* Add support for setting the environment.
* Add support for `uid` and `gid`.
* Possibly add support for `chdir`.
* Possibly add support for `chroot`.
* Possibly create backoff algorithm for quarantined children in stead of
  quarantining them a constant time.
* Possibly use higher resolution timers for childrens uptime with
  `clock_gettime(CLOCK_MONOTONIC)`.
* Possibly add support for updating configurations gracefully:
  - Possible solutions:
    - Update child struct, kill, wait and respawn.
    - Update struct for quarantined childs only.
  - Update usage instructions.
* Possibly add automated tests.

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
* Document requirements (kernel version) for new system calls.
* Upadte portability section.

### 2.1.0

* Create going information utility:
  - Uptime of children.
  - More statistics either from /proc or getrusage().
  - Color output where supported (look into [foreman example][colors]).

### Other

* Possibly logging stdin/sterr with custom log per service.
  - Need to switch handling of SIGCHLD as for `inotify`. If ppoll is used
    for handling inotify we should probably shift to epoll_pwait.
* Asynchronous starting of processes.
* Look into using `scan-build` in debug make target.


[foreman]: http://ddollar.github.com/foreman/
[semantic]: http://semver.org/
[pselect]: http://www.kernel.org/doc/man-pages/online/pages/man2/select.2.html
[ppoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/poll.2.html
[epoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/epoll_wait.2.html
[signalfd]: http://www.kernel.org/doc/man-pages/online/pages/man2/signalfd.2.html
[race]: http://www.linuxprogrammingblog.com/code-examples/using-pselect-to-avoid-a-signal-race
[select_tut]: http://www.kernel.org/doc/man-pages/online/pages/man2/select_tut.2.html
[colors]: http://wynnnetherland.com/journal/a-stylesheet-author-s-guide-to-terminal-colors
