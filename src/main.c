/*
 * --------------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tomenglund26@gmail.com> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return. Tom Englund
 * --------------------------------------------------------------------------------
 */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>


#define pbufsize 40
#define DRI_PATH "/sys/kernel/debug/dri/"
#define BOOST_FILE "boost"
#define PSTATE_FILE "pstate"

void safefree(void **pp) {
  assert(pp);  /* in debug mode abort if pp is NULL */
  if(pp != NULL) {
    free(*pp);
    *pp = NULL;
  }
}

char *combine(char *str1, char *str2) {
  size_t l = (strlen(str1) + strlen(str2) + 1);
  char *str = malloc(l);

  strcpy(str, str1);
  strcat(str, str2);

  return str;
}

char *get_path(char *file, char *card) {
  size_t l = (strlen(file) + strlen(card) + strlen(DRI_PATH) + 2);
  char *buffer = malloc(l);

  strcpy(buffer, DRI_PATH);
  strcat(buffer, card);
  strcat(buffer, "/");
  strcat(buffer, file);

  return buffer;
}

char *get_cards(int printcards) {
  char *card = NULL;
  DIR *dp;
  struct  dirent *ep;
  dp = opendir(DRI_PATH);

  if(dp != NULL) {
    while((ep = readdir(dp))) {
      if(strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
        if(printcards) {
          fprintf(stdout, "%s \n", ep->d_name);
        }
        else
        {
          DIR *cdp;
          struct dirent *cep;
          char *path = combine(DRI_PATH, ep->d_name);
          cdp = opendir(path);

          if(cdp != NULL) {
            while((cep = readdir(cdp))) {
              if(strcmp(cep->d_name, "boost") == 0 || strcmp(cep->d_name, "pstate")) {
                card = malloc(strlen(ep->d_name) + 1);
                strcpy(card, ep->d_name);
                break;
              }
            }

            closedir(cdp);
          }

          safefree((void **)&path);

          if(card != NULL) {
            break;
          }
        }
      }
    }

    closedir(dp);
  }
  else {
    fprintf(stderr, "Couldnt open the directory : %s \n", DRI_PATH);
  }

  return card;
}

void print_file(char *file, char *card) {
  char *path = get_path(file, card);
  FILE *fp;
  char buffer[pbufsize];
  fp = fopen(path, "r");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", path);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  while(fgets(buffer, pbufsize, fp) != NULL) {
    fprintf(stdout, "%s", buffer);
  }

  fclose(fp);
  fp = NULL;
  safefree((void **)&path);
}

void set_val(char *file, char *card, char *val) {
  char *path = get_path(file, card);
  FILE *fp;

  fp = fopen(path, "w");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", path);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  fprintf(fp, "%s\n", val);

  fclose(fp);
  fp = NULL;
  safefree((void **)&path);
}

void print_clocks(char *file, char *card) {
  char *path = get_path(file, card);
  FILE *fp;
  char buffer[pbufsize];
  fp = fopen(path, "r");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", path);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  char *acline = "AC: ";
  char *dcline = "DC: ";
  char *unkline ="--: ";
  size_t len = strlen(acline); //only needs one len, same amount of chars on all three.

  while(fgets(buffer, pbufsize, fp) != NULL) {
    if(strncmp(acline, buffer, len) == 0 || strncmp(dcline, buffer, len) == 0 || strncmp(unkline, buffer, len) == 0) {
      char line[pbufsize];
      for(int i = len; i < pbufsize; i++)
        line[i-len] = buffer[i];

      fprintf(stdout, "%s", line);
    }
  }

  fclose(fp);
  fp = NULL;
  safefree((void **)&path);
}

void print_help() {
  fprintf(stdout, "Usage: nouveauctl [options]\n\n");
  fprintf(stdout, " Options:\n");
  fprintf(stdout, "  -bVALUE, --boost=VALUE     set boost to VALUE, if no VALUE is provided print the file.\n");
  fprintf(stdout, "  -cVALUE, --card=VALUE      sets which card to use in case of nvidia not being first,\n");
  fprintf(stdout, "                              defaults to first found with a pstate and/or boost file,\n");
  fprintf(stdout, "                              if no VALUE is provided print list of available DRI cards.\n\n");
  fprintf(stdout, "  -pVALUE, --pstate=VALUE    set pstate to VALUE, if no VALUE is provided print the file.\n");
  fprintf(stdout, "  -g, --get-clocks           print 'core {HZ} MHz memory {HZ} MHz' of current pstate. to make things\n");
  fprintf(stdout, "                              easier for bash scripts parsing.\n\n");
  fprintf(stdout, "  -h, --help                 print this help and exit\n\n");
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  struct option long_opt[] = {
    {"boost", optional_argument, 0, 'b'},
    {"card", optional_argument, 0, 'c'},
    {"pstate", optional_argument, 0, 'p'},
    {"get-clocks", no_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  int printcards = 0;
  int printboost = 0;
  int printpstate = 0;
  int printclocks = 0;
  int alloccard = 0;
  char *cardarg = NULL;
  char *boostarg = NULL;
  char *pstatearg = NULL;

  int option_index = 0;
  int opt;

  while((opt = getopt_long(argc, argv, "c::b::p::gh", long_opt, &option_index)) != -1) {
    switch(opt) {
      case 'b':
        if(optarg) {
          boostarg = optarg;
        }
        else {
          printboost = 1;
        }
      break;

      case 'c':
        if(optarg) {
          cardarg = optarg;
        }
        else {
          printcards = 1;
        }
      break;

      case 'p':
        if(optarg) {
          pstatearg = optarg;
        }
        else {
          printpstate = 1;
        }
      break;

      case 'g':
        printclocks = 1;
      break;

      default:
        print_help();
        exit(EXIT_SUCCESS);
      break;
    }
  }

  if(argc == 1) {
    print_help();
    exit(EXIT_SUCCESS);
  }

  if(geteuid() != 0) {
    fprintf(stderr, "Error you cant perform this without you being root. \n");
    exit(EXIT_FAILURE);
  }

  if(cardarg == NULL) {
    cardarg = get_cards(0);
    alloccard = 1;
  }

  if(printboost) {
    fprintf(stdout, "Boost Entrys on DRI card: %s\n", cardarg);
    print_file(BOOST_FILE, cardarg);
  }
  else if(boostarg != NULL) {
    set_val(BOOST_FILE, cardarg, boostarg);
  }

  if(printpstate) {
    fprintf(stdout, "Pstate Entrys on DRI card: %s\n", cardarg);
    print_file(PSTATE_FILE, cardarg);
  }
  else if(pstatearg != NULL)
  {
    set_val(PSTATE_FILE, cardarg, pstatearg);
  }

  if(printcards) {
    fprintf(stdout, "Available DRI cards: \n");
    get_cards(1);
  }

  if(printclocks) {
    print_clocks(PSTATE_FILE, cardarg);
  }

  if(alloccard) {
    safefree((void **)&cardarg);
  }

  return EXIT_SUCCESS;
}


