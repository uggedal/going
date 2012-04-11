#define _GNU_SOURCE
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

#define IDENT "going"

#define CHILD_NAME_SIZE 32
#define CHILD_CMD_SIZE 128

#define CONFIG_DIR "/etc/going.d"
#define CONFIG_LINE_BUFFER_SIZE 256
#define CONFIG_CMD_KEY "cmd"

#define	RESPAWN_SPACING 5
#define	RESPAWN_SLEEP 30


struct Child {
  char name[CHILD_NAME_SIZE+1];
  char cmd[CHILD_CMD_SIZE+1];
  pid_t pid;
  time_t up_at;
  struct Child *next;
};

static struct Child *head_ch = NULL;

static int only_files_selector(const struct dirent *d) {
  return strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0;
}

static bool safe_strcpy(char *dst, const char *src, size_t size) {
  return (unsigned) snprintf(dst, size, "%s", src) < size;
}

static bool str_not_empty(char *str) {
  return strnlen(str, 1) == 1;
}

static void slog(int priority, char *message, ...)
{
  va_list ap;
  sigset_t all_mask, orig_mask;

  sigfillset(&all_mask);
  sigprocmask(SIG_BLOCK, &all_mask, &orig_mask);

  openlog(IDENT, LOG_PID, LOG_DAEMON);
  va_start(ap, message);
  vsyslog(priority, message, ap);
  va_end(ap);
  closelog();

  sigprocmask(SIG_SETMASK, &orig_mask, NULL);
}

static void *safe_malloc(size_t size)
{
  void	*mp;

  while ((mp = malloc(size)) == NULL) {
    slog(LOG_EMERG, "Could not allocate memory on the heap");
    sleep(1);
  }
  memset(mp, 0, size);
  return mp;
}

static bool parse_config(struct Child *ch, FILE *fp, char *name) {
  bool valid = false;
  char buf[CONFIG_LINE_BUFFER_SIZE], *line, *key, *value;

  ch->pid = 0;
  ch->up_at = 0;
  ch->next = NULL;

  if (!safe_strcpy(ch->name, name, sizeof(ch->name))) {
    slog(LOG_ERR, "Configuration file name %s is too long (max: %d)",
         name, CHILD_NAME_SIZE);
    return false;
  }

  while ((line = fgets(buf, sizeof(buf), fp)) != NULL) {
    key = strsep(&line, "=");
    value = strsep(&line, "\n");

    if (key != NULL && value != NULL) {
      if (strcmp(CONFIG_CMD_KEY, key) == 0 && str_not_empty(value)) {
        if (safe_strcpy(ch->cmd, value, sizeof(ch->cmd))) {
          valid = true;
        } else {
          slog(LOG_ERR, "Value of %s= in %s is too long (max: %d)",
               CONFIG_CMD_KEY, name, CHILD_NAME_SIZE);
        }
      }
    }
  }
  return valid;
}

static void parse_confdir(const char *dirpath) {
  struct Child *prev_ch = NULL;
  struct dirent **dirlist;
  int dirn;
  char path[PATH_MAX + 1];
  FILE *fp;

  dirn = scandir(dirpath, &dirlist, only_files_selector, alphasort);
  if (dirn < 0) {
    slog(LOG_ALERT, "Can't open %s: %m", dirpath);
    exit(EX_OSFILE);
  }

  while (dirn--) {
    snprintf(path, PATH_MAX + 1, "%s/%s", dirpath, dirlist[dirn]->d_name);

    if ((fp = fopen(path, "r")) == NULL) {
      slog(LOG_ERR, "Can't read %s: %m", path);
      break;
    }

    struct Child *ch = safe_malloc(sizeof(struct Child));

    if (parse_config(ch, fp, dirlist[dirn]->d_name)) {
      if (prev_ch) {
        prev_ch->next = ch;
      } else {
        head_ch = ch;
      }
      prev_ch = ch;
    } else {
      free(ch);
    }

    fclose(fp);
    free(dirlist[dirn]);
  }

  free(dirlist);
}

static void spawn_child(struct Child *ch) {
  pid_t ch_pid;
  char *argv[16];
  sigset_t empty_mask;

  sigemptyset(&empty_mask);

  argv[0] = basename(ch->cmd);
  argv[1] = NULL;

  while (true) {
    if ((ch_pid = fork()) == 0) {
      sigprocmask(SIG_SETMASK, &empty_mask, NULL);
      // TODO: Should file descriptors 0, 1, 2 be closed or duped?
      // TODO: Close file descriptors which should not be inherited or
      //       use O_CLOEXEC when opening such files.

      time_t now = time(NULL);

      if (ch->up_at > 0 && now >= ch->up_at && now - ch->up_at < RESPAWN_SPACING) {
        slog(LOG_WARNING, "Child %s is exiting too fast: %ds (limit: %ds)",
             ch->name, now - ch->up_at, RESPAWN_SPACING);
        sleep(RESPAWN_SLEEP);
      }

      execvp(ch->cmd, argv);
      slog(LOG_ERR, "Can't execute %s: %m", ch->cmd);
      exit(EXIT_FAILURE);

    } else if (ch_pid == -1) {
      sleep(1);
    } else {
      ch->up_at = time(NULL);
      ch->pid = ch_pid;
      return;
    }
  }
}

static void respawn(void) {
  struct Child *ch;
  int status;
  pid_t ch_pid;

  while ((ch_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (ch = head_ch; ch; ch = ch->next) {
      if (ch_pid == ch->pid) {
        spawn_child(ch);
      }
    }
  }
}

static void cleanup(void) {
  struct Child *tmp_ch;

  while (head_ch != NULL) {
    tmp_ch = head_ch->next;
    free(head_ch);
    head_ch = tmp_ch;
  }
}

static void block_signals(sigset_t *block_mask) {
  sigemptyset(block_mask);
  sigaddset(block_mask, SIGCHLD);
  sigaddset(block_mask, SIGTERM);
  sigaddset(block_mask, SIGINT);
  sigaddset(block_mask, SIGHUP);
  sigprocmask(SIG_BLOCK, block_mask, NULL);
}

static char *parse_args(int argc, char **argv) {
  if (argc == 1) {
    return CONFIG_DIR;
  }

  if (argc == 3 && strcmp("-d", argv[1]) == 0 && str_not_empty(argv[2])) {
    return argv[2];
  }

  fprintf(stderr, "Usage: %s [-d conf.d]\n", argv[0]);
  exit(EX_USAGE);
}

int main(int argc, char **argv) {
  struct Child *ch;
  sigset_t block_mask;
  int sig;

  if (atexit(cleanup) != 0) {
    slog(LOG_ERR, "Unable to register atexit(3) function");
  }

  block_signals(&block_mask);

  // TODO: parse command line arg (-d) and return EX_USAGE on failure.
  // TODO: use default or command line conf.d.

  parse_confdir(parse_args(argc, argv));

  // TODO: What to do if we have no valid children?
  //       - should log this.
  //       - with inotify an empty directory can be valid.

  for (ch = head_ch; ch != NULL; ch = ch->next) {
    spawn_child(ch);
  }

  while (true) {
    sig = sigwaitinfo(&block_mask, NULL);

    if (sig != SIGCHLD) {
      // TODO: Decide if we should re-raise terminating signals.
      break;
    }

    respawn();
  }

  // TODO: Kill and reap children if we have left the SIGCHLD loop.
  //       - What about children respawning too fast (in sleep mode)?

  return EXIT_SUCCESS;
}
