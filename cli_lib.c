#define _XOPEN_SOURCE 700
#include <ctype.h> // tolower
#include <errno.h> // errno
#include <fcntl.h> // F_SETFL, O_NONBLOCK
#include <stdbool.h>
#include <stdio.h> // stdin, EOF
#include <stdlib.h> // EXIT_FAILURE
#include <string.h> // strnlen
#include <strings.h> // strerror
#include <sys/socket.h> // send, recv
#include "cli_lib.h"

/**
 * @brief Attempt to convert a string to a port number.
 * @param[in] p_string A pointer to a string containing the port number to
 *                     convert.
 * @return A valid port number between 0 and 65535
 *         INVALID_PORT if an error occurs.
 */
int convert_port_number(char* p_string)
{
    if (NULL == p_string)
    {
        fprintf(stderr, "No string to get port number from.\n");
        return INVALID_PORT;
    }

    // Truncate input string.
    //
    if (5 < strnlen(p_string, 6))
    {
        p_string[5] = '\0';
    }

    char* p_cursor        = p_string;
    char* p_cursor_memory = NULL;
    
    errno = 0;
    int port_number = strtol(p_cursor, &p_cursor_memory, 10);
    if (0 != errno)
    {
        fprintf(stderr, "[%s]\n", p_string);
        fprintf(stderr,
                "Conversion failed for given port number. [%s]\n",
                strerror(errno));
        return INVALID_PORT;
    }

    if (0 >= port_number || 65535 < port_number)
    {
        fprintf(stderr, 
                "Port number out of range. Must be between 0 and 65535\n");
        port_number = INVALID_PORT;
    }

    return port_number;
} /* convert_port_number */

/**
 * @brief Purges any remaining input on STDIN buffer.
 */
void purge_buffer()
{
    int c;
    fcntl(0, F_SETFL, O_NONBLOCK);
    while ((c = fgetc(stdin)) != '\n' && c != EOF)
    {
        /* continue */
    }
    fcntl(0, F_SETFL, 0);
} /* purge_buffer */

/**
 * @brief Checks a given string against a list of options.
 * @param[in] p_string A pointer to a string to match against options.
 * @return 1 if exit
 *         0 if no option input
 */
int check_for_exit(char *p_string)
{
    if (NULL == p_string)
    {
        fprintf(stderr, "No string provided for option checks.\n");
        return 0;
    }
    // List is built for expandability.
    //
    char* options[] =
    {
        "exit",
        NULL
    };
    for (int i = 0; NULL != options[i]; i++)
    {
        char* opt = options[i];
        int compare = strncmp(opt, p_string, strnlen(opt, MAX_BUFFER_SIZE));
        if (0 == compare)
        {
            return i + 1;
        }
    }
    return 0;
} /* check_for_exit */

/**
 * @brief Send a given postfix string via a given socket.
 * @param[in] p_postfix A pointer to a valid postfix notation string.
 * @param[in] client_socket_fd The socket file descriptor for the client.
 * @return true if successfuly sent
 *         false if connection is closed or connection failed.
 */
bool send_postfix(char* p_postfix, int client_socket_fd)
{
    if (NULL == p_postfix)
    {
        fprintf(stderr, "No postfix string provided to send to server.\n");
        return false;
    }

    printf("Sending postfix to server\n");
    int err = send(client_socket_fd,
                   p_postfix,
                   strnlen(p_postfix, MAX_BUFFER_SIZE),
                   0);
    if(strnlen(p_postfix, MAX_BUFFER_SIZE) > err)
    {
        fprintf(stderr,
                "Unable to send message to server. [%s]\n",
                strerror(errno));
        return false;
    }

    char response[MAX_BUFFER_SIZE] = { 0 };
    printf("Waiting for receive\n");
    err = recv(client_socket_fd, &response, MAX_BUFFER_SIZE, 0);
    if (0 == err)
    {
        fprintf(stderr,
                "Connection to server lost. [%s]\n",
                strerror(errno));
        return false;
    }
    else if (0 > err)
    {
        fprintf(stderr,
                "Unable to receive message on socket. [%s]\n",
                strerror(errno));
        return false;
    }

    printf("Server responded with: \n%s\n", response);
    return true;
} /* send_postfix */