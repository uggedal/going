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

* Basic implementation:
  - Exec childs.
  - Listen on `SIGCHLD` and find pid with `waitpid()`.
* Handle zombie childs.
* Reading configurations from `/etc/going.d`.
* Look into [foreman][] format.
* Man page.
* Github project page.
* Use `inotify` to detect new configs.
* Handle signals gracefully.
* Logging `going` events to syslog.
* Possibly logging stdin/sterr with custom log per service.
  - Need to use [pselect(2)][pselect] or [epoll_pwait(2)][epoll]
    to avoid [signal races][race].
* Add support for setting the environment.
* Add support for `uid` and `gid`.
* Possibly add support for `chdir`.
* Possibly add support for `chroot`.

[foreman]: http://ddollar.github.com/foreman/
[pselect]: http://www.kernel.org/doc/man-pages/online/pages/man2/select.2.html
[epoll]: http://www.kernel.org/doc/man-pages/online/pages/man2/epoll_wait.2.html
[race]: http://www.linuxprogrammingblog.com/code-examples/using-pselect-to-avoid-a-signal-race
