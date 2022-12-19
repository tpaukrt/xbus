// **************************************************************************
//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2016-2022 Tomas Paukrt
//
// The command line tool of simple interprocess communication bus
//
// **************************************************************************

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "xbus.h"

// **************************************************************************
// concatenate the program arguments into one string
static const char *concat_argv(int argc, char **argv, int from)
{
  char                  *payload;
  int                   len;
  int                   i;

  // sum lengths of arguments
  len = 0;
  for (i = from; i < argc; i++) {
    len += strlen(argv[i]) + 1;
  }

  // allocate memory for a string
  payload = (char *)malloc(len);
  if (!payload) {
    perror("malloc error");
    exit(EXIT_FAILURE);
  }

  // concatenate arguments into the created string
  payload[0] = '\0';
  for (i = from; i < argc; i++) {
    if (i > from) {
      strcat(payload, "\n");
    }
    strcat(payload, argv[i]);
  }

  // return the created string
  return payload;
}

// **************************************************************************
// the main function
int main(int argc, char **argv)
{
  char                  *topic;
  char                  *payload;

  // process the command
  if (argc > 1) {
    switch (tolower(argv[1][0])) {
      // command "subscribe"
      case 's':
        if (argc > 2) {
          xbus_subscribe(argv[2]);
          while ((payload = xbus_receive(&topic))) {
            printf("[%s]\n%s\n\n", topic, payload);
          }
          return EXIT_SUCCESS;
        }
        break;
      // command "publish"
      case 'p':
        if (argc > 3) {
          xbus_publish(argv[2], concat_argv(argc, argv, 3));
          return EXIT_SUCCESS;
        }
        break;
      // command "write"
      case 'w':
        if (argc > 3) {
          xbus_write(argv[2], concat_argv(argc, argv, 3));
          return EXIT_SUCCESS;
        }
        break;
      // command "read"
      case 'r':
        if (argc > 2) {
          printf("%s\n", xbus_read(argv[2]));
          return EXIT_SUCCESS;
        }
        break;
      // command "list"
      case 'l':
        printf("%s", xbus_list());
        return EXIT_SUCCESS;
    }
  }

  // print help if incorrect parameters were entered
  printf("Usage: %s <command> [arguments]\n"
         "\n"
         "Commands:\n"
         "  subscribe <topic>\n"
         "  publish <topic> <payload>\n"
         "  write <topic> <payload>\n"
         "  read <topic>\n"
         "  list\n",
         basename(argv[0]));

  // terminate the program
  return EXIT_FAILURE;
}
