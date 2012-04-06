#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libgen.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

struct Child {
  char *name;
  char *cmd;
  pid_t pid;
  struct Child *next;
  // TODO: add respawn counter/timer
};

static struct Child *head_ch = NULL;

static sigset_t orig_mask;

void parse_config(void) {
  struct Child *prev_ch = NULL;

  // TODO: actual parsing
  for (int i = 0; i < 5; i++) {
    struct Child *ch = malloc(sizeof(struct Child));
    // TODO: check that we got memory on the heap.

    ch->name = "echo";
    ch->cmd = "./test/echo.sh";
    ch->pid = 0;
    ch->next = NULL;

    if (prev_ch) {
      prev_ch->next = ch;
    } else {
      head_ch = ch;
    }
    prev_ch = ch;
  }
}

void spawn_child(struct Child *ch) {
  pid_t ch_pid;
  char *argv[16];

  argv[0] = basename(ch->cmd);
  argv[1] = NULL;

  while (true) {
    if ((ch_pid = fork()) == 0) {
      sigprocmask(SIG_SETMASK, &orig_mask, NULL);
      // TODO: Should file descriptors 0, 1, 2 be closed or duped?
      // TODO: Close file descriptors which should not be inherited or
      //       use O_CLOEXEC when opening such files.
      execvp(ch->cmd, argv);
      // TODO: Handle error better than exiting child?
      _exit(EXIT_FAILURE);
    } else if (ch_pid == -1) {
      sleep(1);
    } else {
      ch->pid = ch_pid;
      return;
    }
  }
}

void respawn(void) {
  struct Child *ch;
  int status;
  pid_t ch_pid;

  while ((ch_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    // TODO: Handle errors from waitpid() call.

    for (ch = head_ch; ch; ch = ch->next) {
      if (ch_pid == ch->pid) {
        // TODO: Possibly add some data about respawns and add a backoff
        //       algorithm.
        spawn_child(ch);
        // TODO: Remove debug printf():
        printf("respawned: %s (cmd: %s) (pid: %d)\n", ch->name, ch->cmd, ch->pid);
      }
    }
  }
}

void cleanup(void) {
  struct Child *tmp_ch;

  while (head_ch != NULL) {
    tmp_ch = head_ch->next;
    free(head_ch);
    head_ch = tmp_ch;
  }
}

int main(void) {
  struct Child *ch;
  sigset_t chld_mask;

  // TODO: parse command line arg (-d) and return EX_USAGE on failure.
  // TODO: use default or command line conf.d or return EX_OSFILE.

  parse_config();

  sigemptyset(&chld_mask);
  sigaddset(&chld_mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &chld_mask, &orig_mask);

  for (ch = head_ch; ch; ch = ch->next) {
    spawn_child(ch);
    // TODO: Remove debug printf():
    printf("spawned: %s (cmd: %s) (pid: %d)\n", ch->name, ch->cmd, ch->pid);
  }

  while (true) {
    sigwaitinfo(&chld_mask, NULL);
    respawn();
  }

  // TODO: Kill and reap children if we exit abnormally or just let them
  //       become zombies?

  cleanup();

  return EXIT_SUCCESS;
}
