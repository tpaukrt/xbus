// **************************************************************************
//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2016-2022 Tomas Paukrt
//
// The message broker of simple interprocess communication bus
//
// **************************************************************************

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>

// **************************************************************************

#ifdef DEBUG
#define debuglog(...) syslog(LOG_DEBUG, __VA_ARGS__)
#else
#define debuglog(...)
#endif

// **************************************************************************

// UNIX socket name
#define XBUS_SOCKET     "/var/run/xbus.socket"

// maximum packet size
#define XBUS_MAX_SIZE   8192

// **************************************************************************

// stored message
struct message {
  size_t                size;
  char                  *topic;
  char                  *payload;
  struct message        *next_ptr;
};

// message subscription
struct subscribe {
  char                  *topic;
  struct subscribe      *next_ptr;
};

// client data
struct client {
  int                   sk;
  char                  *name;
  struct subscribe      *subscribe_ptr;
  struct client         *next_ptr;
};

// **************************************************************************

// pointer to the beginning of the list of stored messages
static struct message   *first_message_ptr = NULL;

// pointer to the beginning of the list of clients
static struct client    *first_client_ptr  = NULL;

// **************************************************************************
// safe memory allocation
static void *safe_alloc(size_t size)
{
  void                  *ptr;

  // allocate memory
  if (!(ptr = malloc(size))) {
    syslog(LOG_CRIT, "malloc error: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // return a pointer to the allocated memory
  return ptr;
}

// **************************************************************************
// safe string duplication
static char *safe_strdup(const char *str)
{
  char                  *ptr;

  // create a copy of the string
  if (!(ptr = strdup(str))) {
    syslog(LOG_CRIT, "strdup error: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // return a pointer to the copy of the string
  return ptr;
}

// **************************************************************************
// open a UNIX socket
static int open_unix_socket(const char *path)
{
  struct sockaddr_un    addr;
  int                   sk;

  // create a new socket
  if ((sk = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
    syslog(LOG_CRIT, "create socket error: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // bind the socket to the path
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
  unlink(addr.sun_path);
  if (bind(sk, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    syslog(LOG_CRIT, "bind socket error: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // initialize listening for connection requests
  if (listen(sk, 8) != 0) {
    syslog(LOG_CRIT, "listen on socket error: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // return the assigned socket descriptor
  return sk;
}

// **************************************************************************
// find process name for the client
static const char *get_name(struct client *client_ptr)
{
  struct ucred          peercred;
  socklen_t             optlen;
  ssize_t               size;
  char                  name[64];
  char                  *ptr;
  int                   fd;

  // return the process name if already found
  if (client_ptr->name) {
    return client_ptr->name;
  }

  // find the PID of the client
  optlen = sizeof(peercred);
  if (getsockopt(client_ptr->sk, SOL_SOCKET, SO_PEERCRED, &peercred, &optlen) != 0) {
    return "?";
  }

  // read process information
  snprintf(name, sizeof(name), "/proc/%d/status", peercred.pid);
  if ((fd = open(name, O_RDONLY)) < 0) {
    return "?";
  }
  size = read(fd, name, sizeof(name) - 1);
  name[size > 0 ? size : 0] = '\0';
  close(fd);

  // find process name
  strtok(name, "\t");
  if (!(ptr = strtok(NULL, "\n"))) {
    return "?";
  }

  // save and return the found process name
  return client_ptr->name = safe_strdup(ptr);
}

// **************************************************************************
// send the packet to the client
static void send_packet(struct client *client_ptr, const char *topic, const char *payload)
{
  char                  buffer[XBUS_MAX_SIZE];

  // create the packet content
  snprintf(buffer, sizeof(buffer), "%s\n%s", topic, payload);

  // send the packet to the client
  if (send(client_ptr->sk, buffer, strlen(buffer) + 1, MSG_EOR | MSG_DONTWAIT | MSG_NOSIGNAL) < 0) {
    if (errno != ECONNRESET && errno != ECONNREFUSED && errno != EPIPE) {
      syslog(LOG_WARNING, "process %s lost packet", get_name(client_ptr));
    }
  }
}

// **************************************************************************
// create a client record
static void create_client(int sk)
{
  struct client         *this_ptr;

  // create a new record
  this_ptr = (struct client *)safe_alloc(sizeof(*this_ptr));

  // set the content of the new record
  this_ptr->sk            = sk;
  this_ptr->name          = NULL;
  this_ptr->subscribe_ptr = NULL;

  // add the new record to the list of clients
  this_ptr->next_ptr = first_client_ptr;
  first_client_ptr   = this_ptr;

  // write information to the log
  debuglog("process %s connected", get_name(this_ptr));
}

// **************************************************************************
// destroy a client record
static void destroy_client(int sk)
{
  struct client         *prev_ptr;
  struct client         *this_ptr;
  struct subscribe      *next_ptr;
  struct subscribe      *temp_ptr;

  // find a client record according to the socket descriptor
  prev_ptr = NULL;
  this_ptr = first_client_ptr;
  while (this_ptr && this_ptr->sk != sk) {
    prev_ptr = this_ptr;
    this_ptr = this_ptr->next_ptr;
  }

  // stop if a client record was not found
  if (!this_ptr) {
    return;
  }

  // write information to the log
  debuglog("process %s disconnected", get_name(this_ptr));

  // remove an entry from the list of clients
  if (prev_ptr) {
    prev_ptr->next_ptr = this_ptr->next_ptr;
  } else {
    first_client_ptr = this_ptr->next_ptr;
  }

  // destroy the list of subscribed topics
  temp_ptr = this_ptr->subscribe_ptr;
  while (temp_ptr) {
    next_ptr = temp_ptr->next_ptr;
    free(temp_ptr->topic);
    free(temp_ptr);
    temp_ptr = next_ptr;
  }

  // free allocated memory
  if (this_ptr->name) {
    free(this_ptr->name);
  }
  free(this_ptr);
}

// **************************************************************************
// compare the message topic with the regular expression
static int match_topic(const char *topic, const char *regex)
{
  // process the entire regular expression
  while (*regex) {
    if (*regex == '+') {
      regex++;
      while (*topic && *topic != '/') {
        topic++;
      }
    } else if (*regex == '*') {
      return 1;
    } else if (*regex != *topic) {
      return 0;
    } else {
      regex++;
      topic++;
    }
  }

  // return the result according to the number of remaining characters
  return *topic ? 0 : 1;
}

// **************************************************************************
// store a received message
static void store_message(const char *topic, const char *payload)
{
  struct message        *prev_ptr;
  struct message        *this_ptr;
  size_t                size;

  // get length of the payload
  size = strlen(payload);

  // find a record in the list of stored messages
  prev_ptr = NULL;
  this_ptr = first_message_ptr;
  while (this_ptr && strcmp(this_ptr->topic, topic) < 0) {
    prev_ptr = this_ptr;
    this_ptr = this_ptr->next_ptr;
  }

  // update the content of an existing record if was found
  if (this_ptr && !strcmp(this_ptr->topic, topic)) {
    if (size > this_ptr->size) {
      free(this_ptr->payload);
      this_ptr->size    = size;
      this_ptr->payload = safe_strdup(payload);
    } else {
      strcpy(this_ptr->payload, payload);
    }
    return;
  }

  // create a new record
  this_ptr = (struct message *)safe_alloc(sizeof(*this_ptr));

  // set the content of the new record
  this_ptr->size    = size;
  this_ptr->topic   = safe_strdup(topic);
  this_ptr->payload = safe_strdup(payload);

  // add the new record to the list of stored messages
  if (prev_ptr) {
    this_ptr->next_ptr = prev_ptr->next_ptr;
    prev_ptr->next_ptr = this_ptr;
  } else {
    this_ptr->next_ptr = first_message_ptr;
    first_message_ptr  = this_ptr;
  }
}

// **************************************************************************
// find a stored message according to the topic
static struct message *find_stored_message(const char *topic)
{
  struct message        *this_ptr;

  // find a record in the list of stored messages
  this_ptr = first_message_ptr;
  while (this_ptr && strcmp(this_ptr->topic, topic)) {
    this_ptr = this_ptr->next_ptr;
  }

  // return a pointer to the record
  return this_ptr;
}

// **************************************************************************
// send all stored messages for the topic to the client
static void send_stored_messages(struct client *client_ptr, const char *topic)
{
  struct message        *this_ptr;

  // traverse the list of stored messages
  this_ptr = first_message_ptr;
  while (this_ptr) {
    if (*this_ptr->payload && match_topic(this_ptr->topic, topic)) {
      send_packet(client_ptr, this_ptr->topic, this_ptr->payload);
    }
    this_ptr = this_ptr->next_ptr;
  }
}

// **************************************************************************
// send the message to the client if he has subscribed to the topic
static void send_message(struct client *client_ptr, const char *topic, const char *payload)
{
  struct subscribe      *this_ptr;

  // traverse the list of subscribed topics
  this_ptr = client_ptr->subscribe_ptr;
  while (this_ptr) {
    if (match_topic(topic, this_ptr->topic)) {
      send_packet(client_ptr, topic, payload);
      break;
    }
    this_ptr = this_ptr->next_ptr;
  }
}

// **************************************************************************
// send the message to all clients who have subscribed to the topic
static void dispatch_message(struct client *client_ptr, const char *topic, const char *payload)
{
  struct client         *this_ptr;

  // traverse the list of clients
  this_ptr = first_client_ptr;
  while (this_ptr) {
    if (this_ptr != client_ptr) {
      send_message(this_ptr, topic, payload);
    }
    this_ptr = this_ptr->next_ptr;
  }
}

// **************************************************************************
// process the command PUBLISH (publish a message)
static void process_publish(struct client *client_ptr, const char *topic, const char *payload)
{
  // write information to the log
  debuglog("process %s published \"%s\"", get_name(client_ptr), topic);

  // send the message to all clients who have subscribed to the topic
  dispatch_message(client_ptr, topic, payload);
}

// **************************************************************************
// process the command WRITE (publish and store a message)
static void process_write(struct client *client_ptr, const char *topic, const char *payload)
{
  // write information to the log
  debuglog("process %s wrote \"%s\"", get_name(client_ptr), topic);

  // send the message to all clients who have subscribed to the topic
  dispatch_message(client_ptr, topic, payload);

  // store the message
  store_message(topic, payload);
}

// **************************************************************************
// process the command READ (read a stored message)
static void process_read(struct client *client_ptr, const char *topic)
{
  struct message        *this_ptr;

  // write information to the log
  debuglog("process %s read \"%s\"", get_name(client_ptr), topic);

  // find a stored message according to the topic
  this_ptr = find_stored_message(topic);

  // send the stored message to the client
  send_packet(client_ptr, topic, this_ptr ? this_ptr->payload : "");
}

// **************************************************************************
// process the command SUBSCRIBE (subscribe to a topic)
static void process_subscribe(struct client *client_ptr, const char *topic)
{
  struct subscribe      *this_ptr;

  // write information to the log
  debuglog("process %s subscribed to \"%s\"", get_name(client_ptr), topic);

  // create a new record
  this_ptr = (struct subscribe *)safe_alloc(sizeof(*this_ptr));

  // set the content of the new record
  this_ptr->topic = safe_strdup(topic);

  // add the new record to the list of subscribed topics
  this_ptr->next_ptr        = client_ptr->subscribe_ptr;
  client_ptr->subscribe_ptr = this_ptr;

  // send all stored messages for the topic to the client
  send_stored_messages(client_ptr, topic);
}

// **************************************************************************
// process the command UNSUBSCRIBE (unsubscribe from a topic)
static void process_unsubscribe(struct client *client_ptr, const char *topic)
{
  struct subscribe      *prev_ptr;
  struct subscribe      *this_ptr;

  // find a subscription record according to the topic
  prev_ptr = NULL;
  this_ptr = client_ptr->subscribe_ptr;
  while (this_ptr && strcmp(this_ptr->topic, topic)) {
    prev_ptr = this_ptr;
    this_ptr = this_ptr->next_ptr;
  }

  // stop if a subscription record was not found
  if (!this_ptr) {
    return;
  }

  // write information to the log
  debuglog("process %s unsubscribed from \"%s\"", get_name(client_ptr), topic);

  // remove an entry from the list of subscriptions
  if (prev_ptr) {
    prev_ptr->next_ptr = this_ptr->next_ptr;
  } else {
    client_ptr->subscribe_ptr = this_ptr->next_ptr;
  }

  // free allocated memory
  free(this_ptr->topic);
  free(this_ptr);
}

// **************************************************************************
// process the command LIST (read the list of stored messages)
static void process_list(struct client *client_ptr)
{
  char                  payload[XBUS_MAX_SIZE];
  struct message        *this_ptr;

  // prepare the message payload
  payload[0] = '\0';

  // traverse the list of stored messages
  this_ptr = first_message_ptr;
  while (this_ptr && strlen(payload) + strlen(this_ptr->topic) < sizeof(payload) - 8) {
    strcat(payload, this_ptr->topic);
    strcat(payload, "\n");
    this_ptr = this_ptr->next_ptr;
  }

  // send the packet to the client
  send_packet(client_ptr, "%list", payload);
}

// **************************************************************************
// receive and process a packet from a client
static void receive_packet(struct client *client_ptr)
{
  char                  buffer[XBUS_MAX_SIZE];
  const char            *command;
  const char            *topic;
  const char            *payload;
  ssize_t               size;

  // receive a packet from the client
  size = recv(client_ptr->sk, buffer, sizeof(buffer), MSG_DONTWAIT | MSG_NOSIGNAL);

  // close the connection and destroy all client's record if the client has disconnected
  if (size <= 0) {
    close(client_ptr->sk);
    destroy_client(client_ptr->sk);
    return;
  }

  // terminate processing if the client sent a too long packet
  if (size == sizeof(buffer)) {
    syslog(LOG_WARNING, "process %s sent too long packet", get_name(client_ptr));
    return;
  }

  // terminate the content of the packet
  buffer[size] = '\0';

  // split the packet to parts
  command = strtok(buffer, " \n");
  topic   = strtok(NULL  , "\n" );
  payload = strtok(NULL  , ""   );

  // terminate processing if the client sent a malformed packet
  if (!topic || !*topic) {
   syslog(LOG_WARNING, "process %s sent malformed packet", get_name(client_ptr));
   return;
  }

  // fix empty payload
  if (!payload) {
    payload = "";
  }

  // process the received command
  if (!strcmp(command, "PUBLISH")) {
    process_publish(client_ptr, topic, payload);
  } else if (!strcmp(command, "WRITE")) {
    process_write(client_ptr, topic, payload);
  } else if (!strcmp(command, "READ")) {
    process_read(client_ptr, topic);
  } else if (!strcmp(command, "SUBSCRIBE")) {
    process_subscribe(client_ptr, topic);
  } else if (!strcmp(command, "UNSUBSCRIBE")) {
    process_unsubscribe(client_ptr, topic);
  } else if (!strcmp(command, "LIST")) {
    process_list(client_ptr);
  }
}

// **************************************************************************
// the main function
int main(void)
{
  struct passwd         *pw_ptr;
  struct client         *this_ptr;
  struct client         *next_ptr;
  fd_set                read_fd_set;
  int                   sk_listen;
  int                   sk_temp;
  int                   sk_max;

  // create a new session
  setsid();

  // open the system log
  openlog("xbusd", LOG_PID, LOG_DAEMON);

  // open the UNIX socket
  sk_listen = open_unix_socket(XBUS_SOCKET);

  // drop privileges if possible
  pw_ptr = getpwnam("daemon");
  if (pw_ptr) {
    if (setgid(pw_ptr->pw_gid)) {
      syslog(LOG_ERR, "setgid error: %s", strerror(errno));
    }
    if (setuid(pw_ptr->pw_uid)) {
      syslog(LOG_ERR, "setuid error: %s", strerror(errno));
    }
  }

  // the main loop
  while (1) {

    // assemble a set of sockets
    FD_ZERO(&read_fd_set);
    FD_SET(sk_listen, &read_fd_set);
    sk_max = sk_listen;
    this_ptr = first_client_ptr;
    while (this_ptr) {
      FD_SET(this_ptr->sk, &read_fd_set);
      if (this_ptr->sk > sk_max) {
        sk_max = this_ptr->sk;
      }
      this_ptr = this_ptr->next_ptr;
    }

    // wait for an event
    if (select(sk_max + 1, &read_fd_set, NULL, NULL, NULL) < 0) {
      syslog(LOG_ERR, "select error: %s", strerror(errno));
      continue;
    }

    // accept a new connection
    if (FD_ISSET(sk_listen, &read_fd_set)) {
      if ((sk_temp = accept(sk_listen, NULL, NULL)) >= 0) {
        create_client(sk_temp);
      } else {
        syslog(LOG_ERR, "accept error: %s", strerror(errno));
      }
    }

    // receive commands from clients
    this_ptr = first_client_ptr;
    while (this_ptr) {
      next_ptr = this_ptr->next_ptr;
      if (FD_ISSET(this_ptr->sk, &read_fd_set)) {
        receive_packet(this_ptr);
      }
      this_ptr = next_ptr;
    }

  }

  // terminate the program
  return EXIT_SUCCESS;
}
