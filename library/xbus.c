// **************************************************************************
//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2016-2022 Tomas Paukrt
//
// The library of simple interprocess communication bus
//
// **************************************************************************

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "xbus.h"

// **************************************************************************

// UNIX socket name
#define XBUS_SOCKET     "/var/run/xbus.socket"

// maximum packet size
#define XBUS_MAX_SIZE   8192

// **************************************************************************

// socket descriptor
static int xbus_sk = -1;

// **************************************************************************
// concatenate multiple strings (async-signal-safe)
static void concat(char *dst, size_t size, ...)
{
  va_list               ap;
  const char            *src;

  // copy source strings to the target string
  va_start(ap, size);
  while ((src = va_arg(ap, const char *))) {
    while (*src && size > 1) {
      *dst++ = *src++;
      size--;
    }
  }
  va_end(ap);

  // terminate the target string
  *dst = '\0';
}

// **************************************************************************
// connect to the message broker
void xbus_connect(void)
{
  struct sockaddr_un    addr;

  // return if the connection is already established
  if (xbus_sk >= 0) {
    return;
  }

  // create a new socket
  if ((xbus_sk = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
    syslog(LOG_CRIT, "xbus: create socket error: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // connect to the server
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, XBUS_SOCKET, sizeof(addr.sun_path) - 1);
  if (connect(xbus_sk, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    syslog(LOG_CRIT, "xbus: connect socket error: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // enable automatic socket close during successful exec
  fcntl(xbus_sk, F_SETFD, FD_CLOEXEC);
}

// **************************************************************************
// disconnect from the message broker
void xbus_disconnect(void)
{
  char                  buffer[XBUS_MAX_SIZE];

  // return if the connection is not established
  if (xbus_sk < 0) {
    return;
  }

  // disallow further transmissions
  shutdown(xbus_sk, SHUT_WR);

  // receive remaining packets from the message broker
  while (recv(xbus_sk, buffer, sizeof(buffer), MSG_NOSIGNAL) > 0)
    ;

  // close the socket
  close(xbus_sk);

  // invalidate the socket descriptor
  xbus_sk = -1;
}

// **************************************************************************
// send the packet to the message broker
static void xbus_send(const char *command, const char *topic, const char *payload)
{
  char                  buffer[XBUS_MAX_SIZE];

  // connect to the message broker
  xbus_connect();

  // create the packet content
  concat(buffer, sizeof(buffer), command, " ", topic, "\n", payload, NULL);

  // send the packet to the message broker
  if (send(xbus_sk, buffer, strlen(buffer) + 1, MSG_EOR | MSG_NOSIGNAL) < 0) {
    syslog(LOG_CRIT, "xbus: connection terminated");
    exit(EXIT_FAILURE);
  }
}

// **************************************************************************
// subscribe to the particular topic
void xbus_subscribe(const char *topic)
{
  // send the packet SUBSCRIBE
  xbus_send("SUBSCRIBE", topic, "");
}

// **************************************************************************
// unsubscribe from the particular topic
void xbus_unsubscribe(const char *topic)
{
  // send the packet UNSUBSCRIBE
  xbus_send("UNSUBSCRIBE", topic, "");
}

// **************************************************************************
// publish the message
void xbus_publish(const char *topic, const char *payload)
{
  // send the packet PUBLISH
  xbus_send("PUBLISH", topic, payload);
}

// **************************************************************************
// publish and store the message
void xbus_write(const char *topic, const char *payload)
{
  // send the packet WRITE
  xbus_send("WRITE", topic, payload);
}

// **************************************************************************
// read a stored message
char *xbus_read(const char *topic)
{
  // send the packet READ
  xbus_send("READ", topic, "");

  // return the received response
  return xbus_receive(NULL);
}

// **************************************************************************
// get the list of stored messages
char *xbus_list(void)
{
  // send the packet LIST
  xbus_send("LIST", "*", "");

  // return the received response
  return xbus_receive(NULL);
}

// **************************************************************************
// receive a message
char *xbus_receive(char **topic)
{
  static char           buffer[XBUS_MAX_SIZE];
  char                  *ptr;
  ssize_t               size;

  // connect to the message broker
  xbus_connect();

  // receive a packet from the message broker
  if ((size = recv(xbus_sk, buffer, sizeof(buffer), MSG_WAITALL | MSG_NOSIGNAL)) <= 0) {
    syslog(LOG_CRIT, "xbus: connection terminated");
    exit(EXIT_FAILURE);
  }

  // split the packet content
  buffer[sizeof(buffer) - 1] = '\0';
  ptr = strchrnul(buffer, '\n');
  if (*ptr) {
    *ptr++ = '\0';
  }

  // return the message topic
  if (topic) {
    *topic = buffer;
  }

  // return the message text
  return ptr;
}

// **************************************************************************
// check pending unread messages
int xbus_pending(void)
{
  struct pollfd         pfd;

  // prepare the structure content
  pfd.fd     = xbus_sk;
  pfd.events = POLLIN;

  // detect the presence of unread data
  return poll(&pfd, 1, 0) > 0;
}

// **************************************************************************
// get the socket descriptor
int xbus_socket(void)
{
  // connect to the message broker
  xbus_connect();

  // return the socket descriptor
  return xbus_sk;
}
