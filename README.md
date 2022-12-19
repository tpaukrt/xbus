# Simple Interprocess Communication Bus

## Introduction

  The xbus is a simple interprocess communication bus based on
  publish/subscribe messaging over a UNIX domain socket that was
  designed for use in embedded devices with limited memory.

  It consists of a daemon that acts as a message broker, a tiny
  library that provides basic functions for communication and
  a command line tool that is useful for debugging as well as for
  writing shell scripts.

## Examples

### Publisher

  ```C
  #include "xbus.h"

  int main(void)
  {
    xbus_publish("sms/incoming", "Hello, World!");

    return 0;
  }
  ```

### Subscriber

  ```C
  #include <stdio.h>
  #include "xbus.h"

  int main(void)
  {
    char *topic, *payload;

    xbus_subscribe("sms/*");

    while (1) {
      payload = xbus_receive(&topic);
      printf("[%s]\n%s\n\n", topic, payload);
    }
  }
  ```

## Prerequisites

  * GNU Make 3.81+
  * GCC 4.9+

## Build instructions

  Execute the following command:

  ```
  make [DEBUG=1] [ASAN=1] [UBSAN=1] [V=1]
  ```

## Install instructions

  Execute the following command:

  ```
  make install [DESTDIR=<dir>]
  ```

## Directory structure

  ```
  xbus
   |
   |--OBJ.*                     Output directories with built files
   |
   |--library                   IPC bus library (client API)
   |
   |--server                    IPC bus server (message broker)
   |
   |--tool                      IPC bus command line tool
   |
   |--LICENSE                   BSD 3-clause license
   |--Makefile                  Main Makefile
   |--NEWS.md                   Version history
   |--README.md                 This file
   |--Rules.mk                  Build rules
   +--Setup.mk                  Build setup
  ```

## License

  The code is available under the BSD 3-clause license.
  See the `LICENSE` file for the full license text.
