// TODO: Intro here. C99, Linux spcific, bla, bla.
// TODO: Connect the paragraphs so that it reads well.

// Dependencies
// ------------

// TODO: doc includes, intro, and what we use from each?
#define _GNU_SOURCE // TODO: Why?
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <sysexits.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/wait.h>


// Constants and globals
// ---------------------

// `syslog(3)` needs an identity.
#define IDENT "going"

// Short usage instructions if you fail at typing. The `VERSION` macro is
// included by giving the `-DVERSION=N.N.N` flag to the compiler.
#define USAGE \
  "going " VERSION " (c) 2012 Eivind Uggedal\n" \
  "usage: going [-d conf.d]\n"

// The sizes of our childrens' members. By using constant sizes we only
// have to `malloc(3)` our entire child structure once per child.
#define CHILD_NAME_SIZE 32
#define CHILD_CMD_SIZE 256
#define CHILD_ARGV_LEN CHILD_CMD_SIZE/2

// Configuration file specifics like the default place to look for
// configurations, the size of the buffer we use to read configuration
// lines, and the keys of our configuration format.
#define CONFIG_DIR "/etc/going.d"
#define CONFIG_LINE_BUFFER_SIZE CHILD_CMD_SIZE+32
#define CONFIG_CMD_KEY "cmd"

// Bad children which terminates before the limit we set here should be
// quarantined accordingly.
#define	QUARANTINE_LIMIT 5
static struct timespec QUARANTINE_TIME = {30, 0};

// If our system fails at giving us resources for `malloc(3)` or `fork(3)`
// we'll have to wait a little.
#define EMERG_SLEEP 1


// The `child_t` type holds information for a child under supervision. We
// identify it based on the name of its configuration file, parse its
// command line including arguments, track its process id, the last time
// it was started, and if it has been quarantined for terminating too fast.
// By having a pointer to the next child we get a nice lightweight linked
// list of children.
typedef struct going_child {
  char name[CHILD_NAME_SIZE+1];
  char cmd[CHILD_CMD_SIZE+1];
  pid_t pid;
  time_t up_at;
  bool quarantined;
  struct going_child *next;
} child_t;

// When iterating our linked list of children we need to have a pointer
// to the head of the list.
static child_t *head_ch = NULL;


// Utility functions
// -----------------

// A safe inline function for copying a string.
// Returns true if the source string fits within the given size or false
// if it was truncated according to the size argument.
static inline bool safe_strcpy(char *dst, const char *src, size_t size) {
  // We use `snprintf(3)` since `strcpy(3)` can overflow its desination string
  // and `strncpy(3)` does not ensue a terminating null for its destination.
  // If we had `strlcpy(3)` in GLIBC this function would be moot...
  return (unsigned) snprintf(dst, size, "%s", src) < size;
}

// A simple inline function to determine whether a string has any content.
static inline bool str_not_empty(char *str) {
  return strnlen(str, 1) == 1;
}

// A `syslog(3)` convenience wrapper which takes a priority level, a
// log message format, and a variable number of arguments to the message
// format.
static void slog(int priority, char *message, ...)
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

// A wrapper arround `malloc(3)` which blocks until we get a pointer to the
// requested memory on the heap.
static void *safe_malloc(size_t size)
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

// Children handling
// -----------------

// Terminate a given child by sending it the `SIGTERM` signal.
// TODO: Should we wait on the child to exit, and send it a
// SIGKILL signal if it fails to obey?
static void kill_child(child_t *ch) {
  kill(ch->pid, SIGTERM);
}

// Free the memory consumed by the given child.
static inline void cleanup_child(child_t *ch) {
  free(ch);
}

// Free the memory consumed by our linked list of children.
static void cleanup_children(void) {
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


// Configuration
// -------------

// Parses the given configuration file into the given child structure.
// Returns true if the format of the configuration file was valid and
// false otherwise.
static bool parse_config(child_t *ch, FILE *fp, char *name) {
  char buf[CONFIG_LINE_BUFFER_SIZE], *line, *key, *value;

  ch->pid = 0;
  ch->up_at = 0;
  ch->next = NULL;

  // We set the child as quarantined so that we can use the
  // `spawn_quarantined_children()` function to bring it up.
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
  return false;
}

// Check whether our liked list of children contains the given child
// identified by name.
static inline bool has_child(char *name) {
  for (child_t *ch = head_ch; ch != NULL; ch = ch->next) {
    if (strncmp(ch->name, name, CHILD_NAME_SIZE) == 0) {
      return true;
    }
  }
  return false;
}

// Returns a pointer to the tail child of the global linked list
// of children. Returns null if the head child is null.
static child_t *get_tail_child(void) {
  child_t *tail_ch;

  for (tail_ch = head_ch; tail_ch; tail_ch = tail_ch->next);

  return tail_ch;
}

// Iterates over its given list of configuration files and adds any unseen
// children to our global linked list.
static void add_new_children(const char *dir, struct dirent **dlist, int dn) {
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

// Check whether a given child identified by name still is present in a
// directory listing of our configuration directory.
static inline bool has_config(char *name, struct dirent **dlist, int dn) {
  for (int i = dn - 1; i >= 0; i--) {
    if (strncmp(name, dlist[i]->d_name, CHILD_NAME_SIZE) == 0) {
      return true;
    }
  }
  return false;
}

// Iterates over the global linked list of children and checks that
// each is still present in the given list of configuration files. Those
// without a corresponding configuration file is removed form the linked
// list and the child process is terminated.
static void remove_old_children(struct dirent **dlist, int dn) {
  child_t *prev_ch = NULL;

  // Iterate over all children, point `prev_ch` to the active child
  // right before the next iteration.
  for (child_t *ch = head_ch; ch != NULL; prev_ch = ch, ch = ch->next) {
    // Check if we have a configuration file for this child identified
    // by its name.
    if (!has_config(ch->name, dlist, dn)) {
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

// A filter function used with `scandir(3)` which returns true for
// all files in a directory excluding `.` and `..`.
static int only_files_selector(const struct dirent *d) {
  return strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0;
}

// Reads configuration files from the directory given and adds/removes
// elements to a linked list of children accordingly.
static void parse_confdir(const char *dir) {
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

// Execution of children
// ---------------------

// TODO/FIXME: This function is too long
static void spawn_child(child_t *ch) {
  char cmd_buf[CHILD_CMD_SIZE+1];
  char *argv[CHILD_ARGV_LEN];
  char *cmd_word;
  pid_t ch_pid;
  sigset_t empty_mask;

  sigemptyset(&empty_mask);

  char *cmd_p = strcpy(cmd_buf, ch->cmd);

  int i = 1;
  while ((cmd_word = strsep(&cmd_p, " ")) != NULL) {
    if (*cmd_word != '\0') {
      if (i == 1) {
        argv[0] = cmd_word;
        argv[1] = basename(cmd_word);
      } else {
        argv[i] = cmd_word;
      }
      i++;
    }
  }
  argv[i] = NULL;

  ch->quarantined = false;

  while (true) switch (ch_pid = fork()) {
    case 0:
      sigprocmask(SIG_SETMASK, &empty_mask, NULL);

      // TODO: Should file descriptors 0, 1, 2 be closed or duped?
      // TODO: Close file descriptors which should not be inherited or
      //       use O_CLOEXEC when opening such files.

      execvp(argv[0], argv + 1);
      slog(LOG_ERR, "Can't execute %s: %m", ch->cmd);
      exit(EXIT_FAILURE);
    case -1:
      slog(LOG_EMERG, "Could not fork, sleeping %ds", EMERG_SLEEP);
      sleep(EMERG_SLEEP);
      break;
    default:
      ch->up_at = time(NULL);
      ch->pid = ch_pid;
      return;
  }
}

static bool respawn_terminated_children(void) {
  child_t *ch;
  int status;
  pid_t ch_pid;
  bool success = true;
  time_t now = time(NULL);

  while ((ch_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (ch = head_ch; ch; ch = ch->next) {
      if (ch_pid == ch->pid) {
        if (ch->up_at > 0
            && now >= ch->up_at
            && now - ch->up_at < QUARANTINE_LIMIT) {
          slog(LOG_WARNING, "%s terminated after: %ds (limit: %ds) and " \
              "will be quarantined for %ds", ch->name, now - ch->up_at,
              QUARANTINE_LIMIT, QUARANTINE_TIME.tv_sec);
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

static bool safe_to_respaw_quarantined(child_t *ch) {
  time_t now = time(NULL);

  return !(ch->up_at > 0 && now >= ch->up_at
           && now - ch->up_at < QUARANTINE_TIME.tv_sec);
}


static void spawn_quarantined_children(void) {
  child_t *ch;

  for (ch = head_ch; ch != NULL; ch = ch->next) {
    if (ch->quarantined && safe_to_respaw_quarantined(ch)) {
      spawn_child(ch);
    }
  }
}

// Signal handling
// ---------------

static inline void block_signals(sigset_t *block_mask) {
  sigemptyset(block_mask);
  sigaddset(block_mask, SIGCHLD);
  sigaddset(block_mask, SIGTERM);
  sigaddset(block_mask, SIGINT);
  sigaddset(block_mask, SIGHUP);
  sigprocmask(SIG_BLOCK, block_mask, NULL);
}

static void wait_forever(sigset_t *block_mask, const char *confdir) {
  struct timespec *timeout = NULL;

  while (true) switch(sigtimedwait(block_mask, NULL, timeout)) {
    case -1:
      if (errno == EAGAIN) {
        spawn_quarantined_children();
        timeout = NULL;
      }
      break;
    case SIGCHLD:
      if (!respawn_terminated_children()) {
        timeout = &QUARANTINE_TIME;
      }
      spawn_quarantined_children();
      break;
    case SIGHUP:
      parse_confdir(confdir);
      spawn_quarantined_children();
      break;
    default:
      // TODO: Decide if we should re-raise terminating signals.
      // TODO: Kill and reap children if we have left the SIGCHLD loop.
      //       - What about children respawning too fast (in sleep mode)?
      exit(EXIT_FAILURE);
  }
}

// Argument parsing
// ----------------

static inline char *parse_args(int argc, char **argv) {
  if (argc == 1) {
    return CONFIG_DIR;
  }

  if (argc == 3 && strcmp("-d", argv[1]) == 0 && str_not_empty(argv[2])) {
    return argv[2];
  }

  fprintf(stderr, USAGE);
  exit(EX_USAGE);
}

int main(int argc, char **argv) {
  sigset_t block_mask;

  const char *confdir = parse_args(argc, argv);

  atexit(cleanup_children);

  block_signals(&block_mask);

  parse_confdir(confdir);

  spawn_quarantined_children();

  wait_forever(&block_mask, confdir);

  return EXIT_SUCCESS;
}
