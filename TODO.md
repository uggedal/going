TODO
====

1.0.0
-----

* Setup fresh environment for children.
* Start children in their own session.
* Connect stdinn/out/err to /dev/null.
* Literate programming:
  - Link references to library functions in section 3 to online man pages
    on kernel.org.
* GH-pages:
  - Download section:
      - Links to tarball of release tags served by GH.
      - Link into CHANGES document.
      - Checksum.
  - Possibly append analytics tracking with Makefile.
* Look into [foreman][] format.
* Use [semantic versioning][semantic].
* Tag releases in git.
* Create Debian and RPM package (using FPM).
* Post to Arch Linux BBS (Community Contributions).
* Post to Hacker News.
* Post to r/programming, r/linux, r/archlinux, and other.


1.1.0
-----

* Add support for setting the environment.
* Add support for `uid` and `gid`.
* Possibly run each child in its own cgroup so that eventual grandchildren
  can be handled.
* Possibly add support for `chdir`.
* Possibly add support for `chroot`.
* Possible add support for private networking by setting up a network
  namespace with only a loopback interface configured in it.
* Possible support for limiting capabilities.
* Possible support for setting resource limits with `setrlimit(2)`.
* Possible support for file system limiting using file system namespacing.
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
  - Hook up to [Travis CI][travis] and compile on gcc and possibly clang.


2.0.0
-----

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


2.1.0
-----

* Create going information utility:
  - Uptime of children.
  - More statistics either from /proc or getrusage().
  - Color output where supported (look into [foreman example][colors]).


Other
-----

* Possibly logging stdin/sterr with custom log per service.
  - Need to switch handling of SIGCHLD as for `inotify`. If ppoll is used
    for handling inotify we should probably shift to epoll_pwait.
* Asynchronous starting of processes.
* Look into using `scan-build` in debug make target.


[foreman]: http://ddollar.github.com/foreman/
[semantic]: http://semver.org/
[travis]: https://groups.google.com/forum/#!msg/travis-ci/z9JNDGjKz-8/tRL0BpdSY24J
[pselect]: http://www.kernel.org/doc/man-pages/online/pages/man2/select.2.html
[ppoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/poll.2.html
[epoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/epoll_wait.2.html
[signalfd]: http://www.kernel.org/doc/man-pages/online/pages/man2/signalfd.2.html
[race]: http://www.linuxprogrammingblog.com/code-examples/using-pselect-to-avoid-a-signal-race
[select_tut]: http://www.kernel.org/doc/man-pages/online/pages/man2/select_tut.2.html
[colors]: http://wynnnetherland.com/journal/a-stylesheet-author-s-guide-to-terminal-colors
