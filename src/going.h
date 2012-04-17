// Header file for the [`going.c` source file](going.c.html).

// Constants
// ---------

// `syslog(3)` needs an identity.
#define IDENT "going"

// A semantic version.
#define VERSION "0.1.0-beta"

// The command line flag used to change the default configuration directory.
#define CMD_FLAG_CONFDIR "-d"

// Short usage instructions if you fail at typing.
#define USAGE \
  "going " VERSION " (c) 2012 Eivind Uggedal\n" \
  "usage: going [" CMD_FLAG_CONFDIR " conf.d]\n"

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


// Types
// -----

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


// Prototypes
// ----------

// Defining prototypes of our functions makes layout out the program
// in a literate style possible.

// Argument parsing
char *parse_args(int argc, char **argv);

// Configuration
void parse_confdir(const char *dir);
void add_new_children(const char *dir, struct dirent **dlist, int dn);
void remove_old_children(struct dirent **dlist, int dn);
bool parse_config(child_t *ch, FILE *fp, char *name);

// Execution of children
void spawn_unquarantined_children(void);
bool can_be_unquarantined(child_t *ch);
bool respawn_terminated_children(void);
void spawn_child(child_t *ch);

// Signal handling
void block_signals(sigset_t *block_mask);
void wait_forever(sigset_t *block_mask, const char *confdir);

// Children handling
child_t *get_tail_child(void);
bool has_child(char *name);
bool child_active(char *name, struct dirent **dlist, int dn);
void kill_child(child_t *ch);
void cleanup_children(void);
void cleanup_child(child_t *ch);

// Utility functions
bool str_not_empty(char *str);
bool safe_strcpy(char *dst, const char *src, size_t size);
void *safe_malloc(size_t size);
int only_files_selector(const struct dirent *d);
void slog(int priority, char *message, ...);
