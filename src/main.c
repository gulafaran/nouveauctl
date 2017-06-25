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
#define BOOST_FILE "/boost"
#define PSTATE_FILE "/pstate"
#define HWMON_PATH "/sys/class/hwmon/"
#define HWMON_TEMP_FILE "/temp1_input"

struct nouveau {
  char *dri_path;
  char *hwmon_path;
  char *pstate_file;
  char *boost_file;
  int gpu_temp;
  char *current_clocks;
};

struct optargs {
  char *pstate;
  char *boost;
  char *card;
  int printclocks;
  int printpstate;
  int printboost;
  int printcards;
  int printtemp;
};

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

  if(card == NULL)
    card = "Unknown";

  return card;
}

int hwmon_isnouveau(char *path) {
  FILE *fp;
  char buffer[pbufsize];
  char *nv = "nouveau";
  int rval = 0;
  char *filep = combine(path, "/name");

  fp = fopen(filep, "r");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", filep);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  if(fgets(buffer, pbufsize, fp) != NULL) {
    if(strncmp(buffer, nv, strlen(nv)) == 0) {
        rval = 1;
    }
  }

  safefree((void **)&filep);
  fclose(fp);
  return rval;
}

char *get_hwmon() {
  char *hwmon_p = NULL;
  DIR *dp;
  struct dirent *ep;
  dp = opendir(HWMON_PATH);

  if(dp != NULL) {
    while((ep = readdir(dp))) {
      if(strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
        DIR *cdp;
        struct dirent *cep;
        char *path = combine(HWMON_PATH, ep->d_name);
        cdp = opendir(path);

        if(cdp != NULL) {
          while((cep = readdir(cdp))) {
            if(strcmp(cep->d_name, "name") == 0) {
              if(hwmon_isnouveau(path)) {
                hwmon_p = malloc(strlen(path) + strlen(ep->d_name) + 1);
                strcpy(hwmon_p, ep->d_name);
                break;
              }
            }
          }
        }

        closedir(cdp);
        safefree((void **)&path);
        if(hwmon_p != NULL)
          break;
      }
    }

    closedir(dp);
  }
  else {
    fprintf(stderr, "Couldnt open the directory : %s \n", HWMON_PATH);
  }

  if(hwmon_p == NULL)
    hwmon_p = "Unknown";

  return hwmon_p;
}

int get_temp(char *path) {
  FILE *fp;
  char buffer[pbufsize];
  int rval = 0;
  char *filep = combine(path, HWMON_TEMP_FILE);

  fp = fopen(filep, "r");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", filep);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  if(fgets(buffer, pbufsize, fp) != NULL) {
    rval = strtol(buffer, (char **)NULL, 10);
    rval = rval / 1000;
  }

  safefree((void **)&filep);
  fclose(fp);
  return rval;
}

char *get_clk(char *path) {
  FILE *fp;
  char buffer[pbufsize] = {0};
  char *filep = combine(path, PSTATE_FILE);
  char *acline = "AC: ";
  char *dcline = "DC: ";
  char *unkline = "--: ";
  char *str = NULL;

  size_t len = strlen(acline); //only needs one len, same amount of chars on all three.

  fp = fopen(filep, "r");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", filep);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  while(fgets(buffer, pbufsize, fp) != NULL) {
    if(strncmp(acline, buffer, len) == 0 || strncmp(dcline, buffer, len) == 0 || strncmp(unkline, buffer, len) == 0) {
      char line[pbufsize];
      for(int i = len; i < pbufsize; i++) {
        if(buffer[i] == '\n')
          line[i-len] = '\0';
        else
          line[i-len] = buffer[i];
      }

      size_t l = strlen(line) + 1;
      str = malloc(l);
      strcpy(str, line);
    }
  }

  safefree((void **)&filep);
  fclose(fp);

  return str;
}

void print_file(char *file) {
  FILE *fp;
  char buffer[pbufsize];
  fp = fopen(file, "r");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", file);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  while(fgets(buffer, pbufsize, fp) != NULL) {
    fprintf(stdout, "%s", buffer);
  }

  fclose(fp);
}

void write_tofile(char *file, char *val) {
  FILE *fp;

  fp = fopen(file, "w");

  if(!fp) {
    fprintf(stderr, "Error while opening: %s\n", file);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  fprintf(fp, "%s\n", val);

  fclose(fp);
}

void fill_struct(struct nouveau *nv, char *c) {
  char *cards;

  if(c)
    cards = c;
  else
    cards = get_cards(0);

  char *hwmon = get_hwmon();

  nv->dri_path = combine(DRI_PATH, cards);
  nv->hwmon_path = combine(HWMON_PATH, hwmon);
  nv->pstate_file = combine(nv->dri_path, PSTATE_FILE);
  nv->boost_file = combine(nv->dri_path, BOOST_FILE);
  nv->gpu_temp = get_temp(nv->hwmon_path);
  nv->current_clocks = get_clk(nv->dri_path);

  safefree((void **)&cards);
  safefree((void **)&hwmon);
}

void handle_opts(struct optargs arg, struct nouveau nv) {
  if(arg.pstate)
    write_tofile(nv.pstate_file, arg.pstate);
  if(arg.boost)
    write_tofile(nv.boost_file, arg.boost);
  if(arg.printclocks)
    fprintf(stdout, "%s \n", nv.current_clocks);
  if(arg.printpstate)
    print_file(nv.pstate_file);
  if(arg.printboost)
    print_file(nv.boost_file);
  if(arg.printcards)
    get_cards(1);
  if(arg.printtemp)
    fprintf(stdout, "%i°C \n", nv.gpu_temp);
}

void free_struct(struct nouveau *nv) {
  if(nv->dri_path)
    safefree((void **)&nv->dri_path);
  if(nv->hwmon_path)
    safefree((void **)&nv->hwmon_path);
  if(nv->pstate_file)
    safefree((void **)&nv->pstate_file);
  if(nv->boost_file)
    safefree((void **)&nv->boost_file);
  if(nv->current_clocks)
    safefree((void **)&nv->current_clocks);
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
  fprintf(stdout, "  -t, --get-temp             print gpu temp in °C.\n");
  fprintf(stdout, "  -h, --help                 print this help and exit\n\n");
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  struct option long_opt[] = {
    {"boost", optional_argument, 0, 'b'},
    {"card", optional_argument, 0, 'c'},
    {"pstate", optional_argument, 0, 'p'},
    {"get-clocks", no_argument, 0, 'g'},
    {"get-temp", no_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  struct optargs arg;
  arg.pstate = NULL;
  arg.boost = NULL;
  arg.card = NULL;
  arg.printclocks = 0;
  arg.printpstate = 0;
  arg.printboost = 0;
  arg.printcards = 0;
  arg.printtemp = 0;

  int option_index = 0;
  int opt;

  while((opt = getopt_long(argc, argv, "c::b::p::gth", long_opt, &option_index)) != -1) {
    switch(opt) {
      case 'b':
        if(optarg) {
          arg.boost = optarg;
        }
        else {
          arg.printboost = 1;
        }
      break;

      case 'c':
        if(optarg) {
          arg.card = optarg;
        }
        else {
          arg.printcards = 1;
        }
      break;

      case 'p':
        if(optarg) {
          arg.pstate = optarg;
        }
        else {
          arg.printpstate = 1;
        }
      break;

      case 'g':
        arg.printclocks = 1;
      break;

      case 't':
        arg.printtemp = 1;
      break;

      case '?':
      case '0':
        print_help();
      break;

      default:
        print_help();
      break;
    }
  }

  if(argc == 1) {
    print_help();
  }

  if(geteuid() != 0) {
    fprintf(stderr, "Error you cant perform this without you being root. \n");
    exit(EXIT_FAILURE);
  }

  struct nouveau nv;
  fill_struct(&nv, arg.card);
  handle_opts(arg, nv);

  free_struct(&nv);
  return EXIT_SUCCESS;
}


