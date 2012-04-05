going
=====

Ensure your services are still going.


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


Goal
----

* No more than 1024kB VmPeak.
* No configuration of `going`.
* Simple configuration files for services.
* Under 500 actual LOC determined by `cloc`.


Inspiration
-----------

* [suckless](http://hg.suckless.org/).
* [redis](https://github.com/antirez/redis).
* [whistlepig](https://github.com/wmorgan/whistlepig).


TODO
----

### 0.1.0

* Reading configurations from `/etc/going.d`.
* Ability to change config directory with a `-d` argument.
* Logging `going` events to syslog.
* Look into starting services with fresh environment, processgroup,
  stdinn/out/err.
* Create make install target (look into handling prefix without ./configure).
* Document that all services needs to be restarted when adding a new config
  with `kill -s QUIT pid`.
* Man page.
* Github project page.
* Look into [foreman][] format.
* Use [semantic versioning][semantic].

### 0.2.0

* Implement gracefull reloading of configurations by listening on SIGHUP
  and document usage with `kill -s HUP pid`.

### 0.3.0

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

### 0.4.0

* Add support for setting the environment.
* Add support for `uid` and `gid`.
* Possibly add support for `chdir`.
* Possibly add support for `chroot`.

### Other

* Possibly logging stdin/sterr with custom log per service.
  - Need to switch handling of SIGCHLD as for `inotify`. If ppoll is used
    for handling inotify we should probably shift to epoll_pwait.
* Look into using `scan-build` in debug make target.


[foreman]: http://ddollar.github.com/foreman/
[semantic]: http://semver.org/
[pselect]: http://www.kernel.org/doc/man-pages/online/pages/man2/select.2.html
[ppoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/poll.2.html
[epoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/epoll_wait.2.html
[signalfd]: http://www.kernel.org/doc/man-pages/online/pages/man2/signalfd.2.html
[race]: http://www.linuxprogrammingblog.com/code-examples/using-pselect-to-avoid-a-signal-race
[select_tut]: http://www.kernel.org/doc/man-pages/online/pages/man2/select_tut.2.html
