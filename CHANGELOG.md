1.9.1 -- 2012-05-14
-------------------

* Add support for setting the working directory of children
  with the `cwd` configuration key.
* Use unaltered path to binary as second argument to execvp(3) so that
  a full path does not incur lookup in $PATH.

0.9.0 -- 2012-05-13
-------------------

* Initial release.
