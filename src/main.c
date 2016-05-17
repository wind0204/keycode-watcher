//#include "/usr/lib/modules/4.5.4-1-ARCH/build/include/linux/input.h"
#include <linux/input.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <systemd/sd-journal.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#define THE_NAME "keycode_watcher"
#define VERSION "0.0"
#define MESSAGE_001 SD_ID128_MAKE(37,2b,c4,a4,38,d1,4e,b5,b1,bb,44,da,13,97,a3,6b)
#define MESSAGE_ERR SD_ID128_MAKE(e6,af,da,18,fa,00,48,a8,a4,eb,dd,6b,b8,52,77,08)
#define MESSAGE_DBG SD_ID128_MAKE(b5,ec,37,d9,9a,46,4e,a3,84,92,fd,a9,fc,cd,fb,d2)

#define MAX_EVENTS 16
struct epoll_event* events;
struct input_dev* in_dev;

// you've got to use ascii coded configuration files with the limit of 512 bytes per line, so lame huh
#define LEN_TXTBUF 512
int fd;
char txtbuf[LEN_TXTBUF];
char* key_ev_src;

struct sll_node {
  char* cmd;
  uint_fast16_t keycode;
  struct sll_node* next;
};
// singly linked list
struct sll {
  struct sll_node* head;
  struct sll_node* tail;
  uint_fast32_t count;
} sll;
struct sll_node* arrayified_nodes;
#define CODE_FOR_THE_EVENT_SOURCE 1024

void inline init_sll(struct sll* sll);
void inline clean_sll(struct sll* sll);
void inline add_sll(struct sll *restrict sll, struct sll_node *restrict entrant);

void deal_with_errno(const char* txt_explanation, int err_number);

//void signal_handler(int signal);

int main(int argc, char* argv[]) {
  //setlogmask(LOG_UPTO(LOG_NOTICE));
  //openlog(THE_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  //syslog(LOG_INFO, "Starting %s %s", THE_NAME, VERSION);
  sd_journal_send("MESSAGE=Starting %s %s", THE_NAME, VERSION,
      "MESSAGE_ID=%s", SD_ID128_CONST_STR(MESSAGE_001),
      "PRIORITY=6",
      NULL);

  sll.count = sll.head = sll.tail = 0;
  key_ev_src = 0;
  events = 0;
  in_dev = 0;

  pid_t pid, sid;
  
  pid = fork();
  if( pid < 0 ) deal_with_errno("failed to fork", errno);
  if( pid > 0 ) exit(EXIT_SUCCESS);

  umask(0);
  sid = setsid();
  if( sid < 0 ) deal_with_errno("failed to create a new session", errno);
  if( chdir("/") < 0 ) deal_with_errno("failed to change the working directory to '/'", errno);




  if( (fd = fopen("/etc/kw.conf", "r")) == 0 ) {
    deal_with_errno("failed to open /etc/kw.conf", errno);
  } else {
    arrayified_nodes = 0;
    while( fgets(txtbuf, LEN_TXTBUF, fd) != 0 ) {
      char* cur = txtbuf-1;
      char quoting = 0;
      do {
        cur++;
        //ignore after a #
        if( *cur == 0 || *cur == '#' ) {
          cur = 0;
          break;
        }
        //see if it's a whitespace (0x9, 0xa, 0xd, 0x20)
        else if( *cur == ' ' || *cur == '	' || *cur == 0xa || *cur == 0xd ) continue;

        break;
      } while( 1 );
      if( cur == 0 ) continue;

      if( *cur == '\'' || *cur == '"' ) quoting = *cur;

      char* head = cur;
      do {
        cur++;
        if( *cur == 0 ) break;

        if ( quoting ) {
          if ( *cur == quoting ) quoting = 0;
        } else {
          if ( *cur == '\'' || *cur == '"' ) quoting = *cur;
          else if( *cur == '=' ) break;
          //see if it's a whitespace (0x9, 0xa, 0xd, 0x20)
          else if( *cur == ' ' || *cur == '	' || *cur == 0xa || *cur == 0xd ) break;
        }
      } while( 1 );
      if( *cur == 0 ) continue;
      char* tail = cur-1;

      quoting = *cur;
      *cur = 0;
      while( head < tail ) {
        if( *head > '9' || *head < '0' ) head++;
        else break;
      }
      int keycode = atoi(head);
      *cur = quoting;
      quoting = 0;
      if( !keycode ) {
        sd_journal_send("MESSAGE=%s (%s)", "couldn't parse this line of configuration, it should start with a number", txtbuf,
            "MESSAGE_ID=%s", SD_ID128_CONST_STR(MESSAGE_ERR),
            "PRIORITY=5",
            NULL);
        clean_sll(&sll);
        exit(EXIT_FAILURE);
      }

      do {
        cur++;
        //ignore after a #
        if( *cur == 0 || *cur == '#' ) {
          cur = 0;
          break;
        }
        //see if it's a whitespace (0x9, 0xa, 0xd, 0x20)
        else if( *cur == ' ' || *cur == '	' || *cur == 0xa || *cur == 0xd ) continue;
        else if( *cur != '=' ) {
          sd_journal_send("MESSAGE=%s (%s)", "couldn't parse this line of configuration", txtbuf,
              "MESSAGE_ID=%s", SD_ID128_CONST_STR(MESSAGE_ERR),
              "PRIORITY=5",
              NULL);
          clean_sll(&sll);
          exit(EXIT_FAILURE);
        }
        else break;
      } while( 1 );
      if( cur == 0 ) continue;

      do {
        cur++;
        if( *cur == ' ' || *cur == '	' || *cur == 0xa || *cur == 0xd ) continue;
        else break;
      } while( 1 );
      if( *cur == 0 ) continue;

      head = cur;
      tail = 0;
      do {
        cur++;
        if( *cur == 0 ) break;

        if ( quoting ) {
          if ( *cur == quoting ) quoting = 0;
        } else {
          //see if it's a whitespace (0x9, 0xa, 0xd, 0x20)
          if( *cur == ' ' || *cur == '	' || *cur == 0xa || *cur == 0xd ) tail = 1;
          else {
            if( *cur == '#' && tail ) break;
            else if ( *cur == '\'' || *cur == '"' ) {
              quoting = *cur;
            }
            tail = 0;
          }
        }

      } while( 1 );

      *cur = 0;
      do {
        cur--;
        //see if it's a whitespace (0x9, 0xa, 0xd, 0x20)
        if( *cur == ' ' || *cur == '	' || *cur == 0xa || *cur == 0xd ) *cur = 0;
        else break;
      } while( 1 );
      tail = cur+1;

      struct sll_node* entrant = malloc(sizeof(struct sll_node));
      entrant->cmd = malloc(strlen(head));
      memcpy(entrant->cmd, head, strlen(head)+1);
      entrant->keycode = keycode;
      add_sll(&sll, entrant);

      if( keycode == CODE_FOR_THE_EVENT_SOURCE ) {
        sd_journal_send("MESSAGE= CODE_FOR_THE_EVENT_SOURCE = '%s'", entrant->cmd,
            "MESSAGE_ID=%s", SD_ID128_CONST_STR(MESSAGE_DBG),
            "PRIORITY=7",
            NULL);
        arrayified_nodes = 1;
      } else {
        sd_journal_send("MESSAGE=the cmd for keycode %d = '%s'", keycode, entrant->cmd,
            "MESSAGE_ID=%s", SD_ID128_CONST_STR(MESSAGE_DBG),
            "PRIORITY=7",
            NULL);
      }
    }
  }

  if( !arrayified_nodes ) {
    sd_journal_send("MESSAGE=You have to specify the source of keystroke events in your configuration file (e.g. %s)", "/dev/input/by-path/platform-i8042-serio-0-event-kbd",
        "MESSAGE_ID=%s", SD_ID128_CONST_STR(MESSAGE_ERR),
        "PRIORITY=5",
        NULL);
    clean_sll(&sll);
    exit(EXIT_FAILURE);
  }

  arrayified_nodes = malloc((sll.count-1) * sizeof(struct sll_node));
  int i;
  struct sll_node* a_node = sll.head;
  for( i=0; i < sll.count; i++ ) {
    if( a_node->keycode == CODE_FOR_THE_EVENT_SOURCE ) {
      key_ev_src = malloc(1+strlen(a_node->cmd));
      memcpy(key_ev_src, a_node->cmd, 1+strlen(a_node->cmd));
    } else {
      memcpy(a_node, arrayified_nodes+i, sizeof(struct sll_node));
    }
    a_node = a_node->next;
  }
  clean_sll(&sll);



  //file descriptors
  int efd;
  struct epoll_event event;
  in_dev = malloc(sizeof(struct input_dev));

  fd = fopen(key_ev_src, "r");
  if( !fd ) deal_with_errno(key_ev_src, errno);
  i = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(in_dev)), &in_dev);
  if( i == -1 ) deal_with_errno("failed to get the information from the event device", errno);
  //TODO: see if it has every keycode the user wants to react

  efd = epoll_create1(0);
  if( efd == -1 ) deal_with_errno("failed to create an epoll file descriptor", errno);

  event.data.fd = fd;
  event.events = EPOLLIN;
  i = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
  if( i == -1 ) deal_with_errno("failed to register the event device's file descriptor on an epoll file descriptor", errno);


  events = calloc(MAX_EVENTS, sizeof(struct epoll_event));
  while( 1 ) {
    i = epoll_wait(efd, events, MAX_EVENTS, -1);
    //TODO: this is the core!
  }


  epoll_ctl(efd, EPOLL_CTL_DEL, fd, 0);
  close(fd);

  free(key_ev_src);
  free(in_dev);
  free(events);
  //closelog();
}


void deal_with_errno(const char* txt_explanation, int err_number) {
  sd_journal_send("MESSAGE=%s (%s)", txt_explanation, strerror(err_number),
      "MESSAGE_ID=%s", SD_ID128_CONST_STR(MESSAGE_ERR),
      "PRIORITY=5",
      NULL);
  clean_sll(&sll);
  free(key_ev_src);
  free(in_dev);
  free(events);
  exit(EXIT_FAILURE);
}

void inline clean_sll(struct sll* sll) {
  struct sll_node* cur = sll->head;
  struct sll_node* prev = 0;
  do {
    if( cur == 0 ) break;
    
    prev = cur;
    cur = cur->next;
    free(prev->cmd);
    free(prev);
  } while( 1 );
  
  sll->count = sll->head = sll->tail = 0;
}

void inline init_sll(struct sll* sll) {
  sll->count = sll->head = sll->tail = 0;
}

void inline add_sll(struct sll* sll, struct sll_node* entrant) {
  if( sll->tail ) {
    sll->tail->next = entrant;
  } else {
    sll->head = entrant;
  }
  sll->tail = entrant;
  entrant->next = 0;
  sll->count++;
}

/*
void signal_handler(int signal) {
  switch( signal ) {
    case SIGHUP:
      break;
    case SIGINT:
    case SIGTERM:
      break;
  }
}
*/
