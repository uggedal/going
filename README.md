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

* Reading configurations from `/etc/going.d`.
* Look into [foreman][] format.
* Man page.
* Github project page.
* Use `inotify` to detect new configs (and stop services in remove configs).
  - Need to change handling of SIGCHLD to use [pselect(2)][pselect],
    [epoll_pwait(2)][epoll] or [signalfd(2)][signalfd] to avoid
    [signal races][race].
* Logging `going` events to syslog.
  - Need to switch handling of SIGCHLD as for `inotify`.
* Possibly logging stdin/sterr with custom log per service.
* Add support for setting the environment.
* Add support for `uid` and `gid`.
* Possibly add support for `chdir`.
* Possibly add support for `chroot`.

[foreman]: http://ddollar.github.com/foreman/
[pselect]: http://www.kernel.org/doc/man-pages/online/pages/man2/select.2.html
[epoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/epoll_wait.2.html
[signalfd]: http://www.kernel.org/doc/man-pages/online/pages/man2/signalfd.2.html
[race]: http://www.linuxprogrammingblog.com/code-examples/using-pselect-to-avoid-a-signal-race
