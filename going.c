#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CHILDREN 128

struct Child {
  char *name;
  char *cmd;
  pid_t pid;
  struct Child *next;
  // TODO: add respawn counter/timer
};

struct Child *head_ch = NULL;

void establish_handlers() {
  // TODO: Setup SIGCHLD handler.
  // TODO: Do cleaup() on SIGINT, SIGTERM, and other relevant signals.
}

void parse_config() {
  struct Child *prev_ch = NULL;

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

  // TODO: actual parsing
}

void spawn_child(struct Child *ch) {
  pid_t ch_pid;
  char *argv[16];

  argv[0] = basename(ch->cmd);
  argv[1] = NULL;

  for (;;) {
    if ((ch_pid = fork()) == 0) {
      execvp(ch->cmd, argv);
      // TODO: handle error better than exiting child?
      _exit(1);
    } else if (ch_pid == -1) {
      // TODO: backoff algorithm?
      sleep(1);
    } else {
      ch->pid = ch_pid;
      return;
    }
  }
}

void respawn(int signal) {
  int status;
  pid_t ch_pid;

  while ((ch_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    // TODO: Lookup the pid and start the dead child again.
    // TODO: Possibly add some data about respawns and add a backoff algorithm.
  }
}

void cleanup() {
  struct Child *tmp_ch;

  while (head_ch != NULL) {
    tmp_ch = head_ch->next;
    free(head_ch);
    head_ch = tmp_ch;
  }
}

int main(int argc, char *argv[]) {
  establish_handlers();
  parse_config();

  struct Child *ch;

  for (ch = head_ch; ch; ch = ch->next) {
    spawn_child(ch);
    printf("spawned: %s (cmd: %s) (pid: %d)\n", ch->name, ch->cmd, ch->pid);
  }

  // TODO: Serve forever.

  cleanup();

  return 0;
}
