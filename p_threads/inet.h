/*
 * Definitions for TCP and UDP client/server programs.
 */

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define	SERV_TCP_PORT	8347

#define	SERV_HOST_ADDR	"129.130.10.43"  /* Change this to be your host addr */
