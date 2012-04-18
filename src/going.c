// TODO: Intro here. C99, Linux spcific, SIGCHLD based, runner, not
//       monitor, bla, bla.
// TODO: Connect the paragraphs so that it reads well.

// Dependencies
// ------------

// By defining the `_GNU_SOURCE` feature test macro we get access to a safe
// `basename(3)` implementation from `string.h`.
#define _GNU_SOURCE

// Include memory allocation and process control functions and macros like
// `exit(3)`, `atexit(3)`, `calloc(3)`, and `EXIT_SUCCESS`.
#include <stdlib.h>

// Include POSIX operating system functions like
// `fork(3)`, `execvp(3)`, and `sleep(3)`.
#include <unistd.h>

// Include macros giving us access to the boolean types `true` and `false`.
#include <stdbool.h>

// Include string operation functions like
// `strncmp(3)`, `strsep(3)`, `strcpy(3)`, and `strnlen(3)`.
#include <string.h>

// Include core buffered input and output functions and macros like
// `fprintf(3)`, `snprintf(3)`, `fopen(3)`, `fclose(3)`, and `FILE`.
#include <stdio.h>

// Include functions and structures for working with directory entries like
// `scandir(3)` and `struct dirent`. 
#include <dirent.h>

// Include date and time handling functions like `time(3)`.
#include <time.h>

// Include declaration of the global `errno` and matching symbolic constants
// like `EAGAIN`.
#include <errno.h>

// Include pre-defined values to be used with `exit(3)` like
// `EX_USAGE` and `EX_OSFILE`.
#include <sysexits.h>


// Include macros for handling variable arguments like 
// `va_start(3)`, `va_end(3)` and `va_list`.
#include <stdarg.h>

// Include functions and constants for logging to the system log like
// `openlog(3)`, `closelog(3)`, `vsyslog(3)`, `LOG_DAEMON`, `LOG_PID`,
// `LOG_EMERG`, and `LOG_WARNING`.
#include <sys/syslog.h>

// Include functions and symbolic constants for waiting for children like
// `waitpid(3)`, and `WNOHANG`.
// This header implicitly includes `signal.h` which gives us signal
// handling functions, constants, and types like `kill(3)`, `sigemptyset(3)`,
// `sigprocmask(3)`, `sigaddset(3)`, `SIGCHLD`, `SIGHUP` and `sigset_t`.
#include <sys/wait.h>

// Include constants, type definitions, and function prototypes from
// the [`going.h` header file](going.h.html).
#include "going.h"


// We need a global pointer to the head of the linked list of children we're
// going to supervise.
static child_t *head_ch = NULL;


// Entrypoint
// ----------

int main(int argc, char **argv) {
  sigset_t block_mask;

  // First we parse the command line arguments to check for a non-standard
  // configuration directory. If no such argument was given we get the
  // default `/etc/going.d`. If an invalid command line flag was given
  // the parse function will exit this process abnormally.
  const char *confdir = parse_args(argc, argv);

  // We setup our cleanup function as an exit handler which will be
  // called at normal process termination.
  atexit(cleanup_children);

  // The signals we're going to handle in our main loop is blocked.
  block_signals(&block_mask);

  // We parse configuration files in the configuration directory
  // into our global linked list of child structures.
  parse_confdir(confdir);

  // All quarantined (a newly initialized child structure is quarantined by
  // default) children is spawned for the first time.
  spawn_unquarantined_children();

  // We launch our main loop which waits for signals and handles them
  // until it receives a terminating signal and promptly exits this process.
  wait_forever(&block_mask, confdir);

  // This return will never be reached, but it can't hurt.
  return EXIT_SUCCESS;
}


// Argument parsing
// ----------------

// Returns the default configuration directory or a non-standard location
// if the configuration directory command line flag is found.
inline char *parse_args(int argc, char **argv) {

  // If we only have on argument, the name of the binary for this source,
  // we simply return the default configuration directory.
  if (argc == 1) {
    return CONFIG_DIR;
  }

  // If we have two extra arguments, the first matching our  configuration
  // directory flag and the second beeing non-empty, we return the second
  // value as the configuration directory.
  if (argc == 3 && strcmp(CMD_FLAG_CONFDIR, argv[1]) == 0
      && str_not_empty(argv[2])) {
    return argv[2];
  }

  // The user has given and illegal number or type of arguments. The program
  // usage is printed to the standard error stream and we exit abnormally.
  fprintf(stderr, USAGE);
  exit(EX_USAGE);
}


// Configuration
// -------------

// ### Parse configuration directory
// Reads configuration files from the directory given and adds/removes
// elements to a linked list of children accordingly.
void parse_confdir(const char *dir) {
  struct dirent **dlist;

  // We use `scandir(3)` since it gives us a nice sorted list of the
  // files in a directory compared to `opendir(3)`.
  int dn = scandir(dir, &dlist, only_files_selector, alphasort);

  // If the configuration directory is unavailable we're in serious trouble
  // and simply exit.
  if (dn < 0) {
    slog(LOG_ALERT, "Can't open %s: %m", dir);
    exit(EX_OSFILE);
  }

  add_new_children(dir, dlist, dn);
  remove_old_children(dlist, dn);

  // We have to free the heap allocated memory for the directory list
  // initialized by `scandir(3)`.
  while (dn--) {
    free(dlist[dn]);
  }
  free(dlist);
}

// ### Add unseen children
// Iterates over its given list of configuration files and adds any unseen
// children to our global linked list.
void add_new_children(const char *dir, struct dirent **dlist, int dn) {
  child_t *tail_ch = get_tail_child();
  char path[PATH_MAX + 1];
  FILE *fp;

  for (int i = dn - 1; i >= 0; i--) {

    // If we already have a child by the name based on this configuration
    // file we simply skip checking it.
    // TODO: What about updating existing configurations? Either:
    //       - Update child struct, kill, wait and respawn or
    //       - Update struct for quarantined childs only
    if (has_child(dlist[i]->d_name)) {
      continue;
    }

    // Create a full path to this configuration file.
    snprintf(path, PATH_MAX + 1, "%s/%s", dir, dlist[i]->d_name);

    // Try to open the configuration file for reading. If we're unable to
    // open it we skip this configuration.
    if ((fp = fopen(path, "r")) == NULL) {
      slog(LOG_ERR, "Can't read %s: %m", path);
      continue;
    }

    // Allocate memory to hold a child structure for this configuration file.
    child_t *ch = safe_malloc(sizeof(child_t));

    // Try to parse this configuration file into the child structure we
    // recently allocated.
    if (parse_config(ch, fp, dlist[i]->d_name)) {
      // If we successfully parsed this configuration file we have to
      // add the resulting child structure to our linked list of children.
      if (tail_ch) {
        // If we have a non-null tail child we add this child after it.
        tail_ch->next = ch;
      } else {
        // If we don't have a tail child, this child is shall be the
        // head of the global linked list.
        head_ch = ch;
      }
      // This child is now the new tail of the global linked list of
      // children.
      tail_ch = ch;
    } else {
      // If we were unable to parse the configuration we free the allocated
      // memory for the child strucure since we don't longer need it and have
      // no references to it after this function exits.
      cleanup_child(ch);
    }

    // Flush the stream and close the underlying file descriptor for the
    // opened configuration file.
    fclose(fp);
  }
}

// ### Remove obselete children
// Iterates over the global linked list of children and checks that
// each is still present in the given list of configuration files. Those
// without a corresponding configuration file is removed form the linked
// list and the child process is terminated.
void remove_old_children(struct dirent **dlist, int dn) {
  child_t *prev_ch = NULL;

  // Iterate over all children, point `prev_ch` to the active child
  // right before the next iteration.
  for (child_t *ch = head_ch; ch != NULL; prev_ch = ch, ch = ch->next) {
    // Check if we have a configuration file for this child identified
    // by its name.
    if (!child_active(ch->name, dlist, dn)) {
      // If we have no configuration file for this child we have to
      // remove it.
      if (prev_ch) {
        // If we have a child before this child in the global linked list
        // we point that to the child after this one. If this is the
        // last child the child before this will be the new last child.
        prev_ch->next = ch->next;
      } else {
        // If this child is the first in the global linked list we point
        // our global head pointer to the child after this one. If this is
        // the first and last child, the head pointer will be null.
        head_ch = ch->next;
      }

      // We terminate the child process when it no longer has a
      // configuration file.
      kill_child(ch);
      // After terminating the child process we make sure to free the
      // memory its scructure took up on the heap.
      cleanup_child(ch);
    }
  }
}

// ### Parse a configuration file
// Parses the given configuration file into the given child structure.
// Returns true if the format of the configuration file was valid and
// false otherwise.
bool parse_config(child_t *ch, FILE *fp, char *name) {
  char buf[CONFIG_LINE_BUFFER_SIZE], *line, *key, *value;

  ch->pid = 0;
  ch->up_at = 0;
  ch->next = NULL;

  // We set the child as quarantined so that we can use the
  // `spawn_unquarantined_children()` function to bring it up.
  ch->quarantined = true;

  // Since the name member of our child structure has a constant size we have
  // to ensure that the name given from the configuration's filename fits.
  // If the name was too large we return immediately with an invalid status.
  if (!safe_strcpy(ch->name, name, sizeof(ch->name))) {
    slog(LOG_ERR, "Configuration file name %s is too long (max: %d)",
         name, CHILD_NAME_SIZE);
    return false;
  }

  // We iterate over the lines in the configuration file until we reach EOF.
  // For safety we use `fgets(3)` which reads at most one less than the given
  // size of characters into the buffer and terminates it.
  while ((line = fgets(buf, sizeof(buf), fp)) != NULL) {

    // The configuration key is found by searching the line until we find
    // a `=` character. `strsep(3)` returns a pointer equal to the given
    // line pointer and overwrites the `=` character by a terminating `\0`
    // character. `strsep(3)` then updates the given line pointer to point
    // past the token. In a successive call to `strsep(3)` to retrieve the
    // configuration value we search from the new location of the line pointer
    // until end of line.
    key = strsep(&line, "=");
    value = strsep(&line, "\n");

    // If `strsep(3)` did not find the tokens we searched for the returned
    // pointers will be null.
    if (key != NULL && value != NULL) {

      // We check that the configuration key matches the command key and
      // the value contains at least one character.
      if (strcmp(CONFIG_CMD_KEY, key) == 0 && str_not_empty(value)) {

        // If we're able to copy the command value read from the configuration
        // file into the constant sized `cmd` member on our child structure
        // we return immediately with a successfull status.
        if (safe_strcpy(ch->cmd, value, sizeof(ch->cmd))) {
          return true;
        } else {
          slog(LOG_ERR, "Value of %s= in %s is too long (max: %d)",
               CONFIG_CMD_KEY, name, CHILD_CMD_SIZE);
        }
      }
    }
  }
  // If no lines in the configuration file contained a valid command line we
  // return with an invalid status.
  return false;
}


// Execution of children
// ---------------------

// ### Spawn unquarantined children
// Iterates over all our children and spawn them if they currently are
// quarantined and safely can be unquarantined.
// This function can also spawn children for the first time since all
// child structures are initialized as quarantined with a last started
// timestamp of epoch.
void spawn_unquarantined_children(void) {
  child_t *ch;

  for (ch = head_ch; ch != NULL; ch = ch->next) {
    if (ch->quarantined && can_be_unquarantined(ch)) {
      spawn_child(ch);
    }
  }
}

// ### Safe to unquarantine a child?
// Checks whether we can stop quarantining a given child.
bool can_be_unquarantined(child_t *ch) {
  // A child can be unquarantined if it:
  //
  //   * has never been spawned,
  //   * or was last spawned more than or equal to the value of
  //     `QUARANTINE_PERIOD`.
  return !child_recently_spawned(ch, QUARANTINE_PERIOD.tv_sec);
}

// ### Respawn terminated children
// Respawns all terminated children. This function is called
// when we get a `SIGCHLD` signal.
bool respawn_terminated_children(void) {
  child_t *ch;
  int status;
  pid_t ch_pid;
  bool success = true;

  while ((ch_pid = waitpid(-1, &status, WNOHANG)) > 0) {

    for (ch = head_ch; ch; ch = ch->next) {

      if (ch_pid == ch->pid) {
        time_t now = time(NULL);

        if (child_recently_spawned(ch, QUARANTINE_TRIGGER)) {
          slog(LOG_WARNING, "%s terminated after: %ds (limit: %ds) and " \
              "will be quarantined for %ds", ch->name, now - ch->up_at,
              QUARANTINE_TRIGGER, QUARANTINE_PERIOD.tv_sec);
          ch->quarantined = true;
          success = false;
          continue;
        }

        slog(LOG_WARNING, "%s terminated after: %ds",
             ch->name, now - ch->up_at);
        spawn_child(ch);
      }
    }
  }
  return success;
}

// ### Spawn a child
// Spawns the command line of the given child by forking and execing
// a child process of the parent `going` process.
void spawn_child(child_t *ch) {

  // A copy of the command line is made so that we can use `strsep(3)`
  // (which modifies its argument) against it. It is safe to use `strcpy(3)`
  // to copy the command line from our child structure into the buffer since
  // we know that their constant sizes are equal.
  char cmd_buf[CHILD_CMD_SIZE+1];
  char *cmd_p = strcpy(cmd_buf, ch->cmd);

  // To avoid allocating space on the heap for the argument vector we
  // use a constant sized array of pointers into the constant sized
  // `cmd_buf` buffer.
  int i = 1;
  char *argv[CHILD_ARGV_LEN];
  char *cmd_word;

  // We iterate until the pointer returned by `strsep(3)` into our command
  // line buffer is null, meaning no new words sperated by a space was found.
  while ((cmd_word = strsep(&cmd_p, " ")) != NULL) {
    // If the word returned is the null byte we're dealing with repeating
    // spaces, so we skip it.
    if (*cmd_word != '\0') {
      // If we're in our first iteration we setup the two first slots
      // of the argument vector which explains why we initialized the index
      // to one.
      if (i == 1) {
        // The first argument will be the path to the binary we'll spawn.
        argv[0] = cmd_word;
        // The second argument will be the base filename of the path
        // to the binary.
        argv[1] = basename(cmd_word);
      } else {
        // The following arguments will be arguments given to the binary
        // we'll spawn.
        argv[i] = cmd_word;
      }
      i++;
    }
  }
  // The argument vector given to `execvp(3)` needs to be null terminated.
  argv[i] = NULL;

  // The child is no longer quarantined since it obviously got the go-ahead
  // to spawn.
  ch->quarantined = false;

  pid_t ch_pid;

  // We iterate until we get the desired behavior from `fork(3)`.
  while (true) {
    // `fork(3)` creates a new process which is an exact copy of its invoking
    // process. We are inside the child process if the return value is zero.
    if ((ch_pid = fork()) == 0) {
      // We initialize an empty signal mask and block based on it, resulting
      // in blockage of no signals. This is done since the parent process
      // blocks certain signals and the child created by a `fork(3)` inherits
      // its parents block mask.
      sigset_t empty_mask;
      sigemptyset(&empty_mask);
      sigprocmask(SIG_SETMASK, &empty_mask, NULL);

      // TODO: Should file descriptors 0, 1, 2 be closed or duped?
      // TODO: Close file descriptors which should not be inherited or
      //       use O_CLOEXEC when opening such files.

      // `execvp(3)` is used to replace this child process with the binary
      // reciding at the path we give as the first argument. In addition it
      // tries to look up the binary in `$PATH` if a non-absolute path is
      // given. By incrementing the `argv` pointer we give as the second
      // argument the receiver sees it as `argv[1:]`.
      execvp(argv[0], argv + 1);
      // If we reach this code the `execvp(3)` call failed. We log the error
      // and exit this child process. Note that the normal flow in the parent
      // continues, but it will get a `SIGCHLD` signal since one of its
      // children terminated.
      slog(LOG_ERR, "Can't execute %s: %m", ch->cmd);
      exit(EXIT_FAILURE);

    // If the return value of `fork(3)` is greater than zero we're in the
    // parent process.
    } else if (ch_pid > 0) {
      // We note the time that the child process started so that we can track
      // its uptime.
      ch->up_at = time(NULL);
      // Storing the process id of the child process is important so that we
      // know which process failed if we get a `SIGCHLD` signal later.
      ch->pid = ch_pid;
      return;

    // If the return value of `fork(3)` is negative the call did not succeed.
    // We log the error and wait a little before trying again.
    } else {
      slog(LOG_EMERG, "Could not fork, sleeping %ds", EMERG_SLEEP);
      sleep(EMERG_SLEEP);
    }
  }
}


// Signal handling
// ---------------

// ### Block handled signals
// TODO: doc me
inline void block_signals(sigset_t *block_mask) {
  sigemptyset(block_mask);
  sigaddset(block_mask, SIGCHLD);
  sigaddset(block_mask, SIGTERM);
  sigaddset(block_mask, SIGINT);
  sigaddset(block_mask, SIGHUP);
  sigprocmask(SIG_BLOCK, block_mask, NULL);
}

// ### Signal handling loop
// TODO: doc me
void wait_forever(sigset_t *block_mask, const char *confdir) {
  struct timespec *timeout = NULL;

  while (true) switch(sigtimedwait(block_mask, NULL, timeout)) {
    case -1:
      if (errno == EAGAIN) {
        spawn_unquarantined_children();
        timeout = NULL;
      }
      break;
    case SIGCHLD:
      if (!respawn_terminated_children()) {
        timeout = &QUARANTINE_PERIOD;
      }
      spawn_unquarantined_children();
      break;
    case SIGHUP:
      parse_confdir(confdir);
      spawn_unquarantined_children();
      break;
    default:
      // TODO: Decide if we should re-raise terminating signals.
      //       - If we exit abnormally we should call cleanup_child() here.
      // TODO: Kill and reap children if we have left the SIGCHLD loop.
      //       - What about children respawning too fast (in sleep mode)?
      exit(EXIT_FAILURE);
  }
}


// Children handling
// -----------------

// ### Tail child
// Returns a pointer to the tail child of the global linked list
// of children. Returns null if the head child is null.
child_t *get_tail_child(void) {
  child_t *tail_ch;

  for (tail_ch = head_ch; tail_ch; tail_ch = tail_ch->next);

  return tail_ch;
}

// ### Existence of child
// Check whether our liked list of children contains the given child
// identified by name.
inline bool has_child(char *name) {
  for (child_t *ch = head_ch; ch != NULL; ch = ch->next) {
    if (strncmp(ch->name, name, CHILD_NAME_SIZE) == 0) {
      return true;
    }
  }
  return false;
}

// ### Existence of config
// Check whether a given child identified by name still is present in a
// directory listing of our configuration directory.
inline bool child_active(char *name, struct dirent **dlist, int dn) {
  for (int i = dn - 1; i >= 0; i--) {
    if (strncmp(name, dlist[i]->d_name, CHILD_NAME_SIZE) == 0) {
      return true;
    }
  }
  return false;
}

// ### Child spawned recently
// Check whether a child was last spawned less than the given number
// of seconds ago.
bool child_recently_spawned(child_t *ch, int seconds_ago) {
  time_t now = time(NULL);

  // We have to check that:
  //
  //  * the child has been spawned before (indicated by a positive `up_at`
  //    timestamp since its set to zero when a child is initialized),
  //  * and that the difference between the current time and the time
  //    the child was spawned is less than the given number of seconds.
  return ch->up_at > 0 && now >= ch->up_at && now - ch->up_at < seconds_ago;
}

// ### Terminate child
// Terminate a given child by sending it the `SIGTERM` signal.
// TODO: Should we wait on the child to exit, and send it a
// SIGKILL signal if it fails to obey?
void kill_child(child_t *ch) {
  kill(ch->pid, SIGTERM);
}

// ### Remove all children
// Free the memory consumed by our linked list of children.
void cleanup_children(void) {
  child_t *tmp_ch;

  // Iterate until the head pointer of our linked list is null.
  while (head_ch != NULL) {
    // Store a temporary pointer to the next child after the current head
    // of the linked list.
    tmp_ch = head_ch->next;
    // Free the memory the current head of the linked list points to.
    cleanup_child(head_ch);
    // Set the temporary pointer as the current head of the linked list.
    head_ch = tmp_ch;
  }
}

// ### Free memory for a child
// Free the memory consumed by the given child.
inline void cleanup_child(child_t *ch) {
  free(ch);
}


// Utility functions
// -----------------

// ### String content
// A simple inline function to determine whether a string has any content.
inline bool str_not_empty(char *str) {
  return strnlen(str, 1) == 1;
}

// ### Safe string copy
// A safe inline function for copying a string.
// Returns true if the source string fits within the given size or false
// if it was truncated according to the size argument.
inline bool safe_strcpy(char *dst, const char *src, size_t size) {
  // We use `snprintf(3)` since `strcpy(3)` can overflow its desination string
  // and `strncpy(3)` does not ensue a terminating null for its destination.
  // If we had `strlcpy(3)` in GLIBC this function would be moot...
  return (unsigned) snprintf(dst, size, "%s", src) < size;
}

// ### Safe heap allocation
// A wrapper arround `calloc(3)` which blocks until we get a pointer to the
// requested memory on the heap.
void *safe_malloc(size_t size)
{
  void	*mp;

  // We retry until we get a non-null pointer to allocated memory.
  // `calloc(3)` is used in stead of `malloc(3)` so that the continous block
  // of memory we receive a pointer to is all zeroed out.
  while ((mp = calloc(1, size)) == NULL) {
    // We were unable to allocate memory so we log the error and give the
    // system some time to get its act together.
    slog(LOG_EMERG, "Could not malloc, sleeping %ds", EMERG_SLEEP);
    sleep(EMERG_SLEEP);
  }
  return mp;
}

// ### Default directory file selector
// A filter function used with `scandir(3)` which returns true for
// all files in a directory excluding `.` and `..`.
int only_files_selector(const struct dirent *d) {
  return strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0;
}

// ### System logger
// A `syslog(3)` convenience wrapper which takes a priority level, a
// log message format, and a variable number of arguments to the message
// format.
void slog(int priority, char *message, ...)
{
  va_list ap;
  sigset_t all_mask, orig_mask;

  // We initialize a signal mask to contain all signals and block
  // them while we're logging to ensure a signal does not interrupt us. The
  // original signal mask is stored so we can reset it after logging.
  sigfillset(&all_mask);
  sigprocmask(SIG_BLOCK, &all_mask, &orig_mask);

  // We log to the daemon log, typically found in `/var/log/daemon.log` with
  // our ident and process id as prefixes.
  openlog(IDENT, LOG_PID, LOG_DAEMON);

  // We use the `vsyslog(3)` function which takes a `va_list` in stead
  // of iterating over the list ourselves.
  va_start(ap, message);
  vsyslog(priority, message, ap);
  va_end(ap);
  closelog();

  // We reset our signal mask to the one in use before we blocked all signals.
  sigprocmask(SIG_SETMASK, &orig_mask, NULL);
}
