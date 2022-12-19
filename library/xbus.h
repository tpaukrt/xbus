// **************************************************************************
//
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2016-2022 Tomas Paukrt
//
// The library of simple interprocess communication bus
//
// **************************************************************************

#ifndef _XBUS_H_
#define _XBUS_H_

#ifdef __cplusplus
extern "C" {
#endif

// connect to the message broker
extern void xbus_connect(void);

// disconnect from the message broker
extern void xbus_disconnect(void);

// subscribe to the particular topic
extern void xbus_subscribe(const char *topic);

// unsubscribe from the particular topic
extern void xbus_unsubscribe(const char *topic);

// publish the message
extern void xbus_publish(const char *topic, const char *payload);

// publish and store the message
extern void xbus_write(const char *topic, const char *payload);

// read a stored message
extern char *xbus_read(const char *topic);

// get the list of stored messages
extern char *xbus_list(void);

// receive a message
extern char *xbus_receive(char **topic);

// check pending unread messages
extern int xbus_pending(void);

// get the socket descriptor
extern int xbus_socket(void);

#ifdef __cplusplus
}
#endif

#endif
