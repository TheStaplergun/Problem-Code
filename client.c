/** @file client.c
 * 
 * @brief This program will connect to a given ip and port, attempt to convert
 *        a given infix string to postfix string, send the postfix string to
 *        the given server, and print out the answer
 *        from the server.
 *        -i [IPv4 address]
 *        -p [PORT]
 *        -e ["INFIX notation string"] (optional)
 *        If -e is used, the program will run once with the given string.
 *          Otherwise, it will keep running, asking for an infix string.
 *        until [exit] is typed
 * @par
 * COPYRIGHT NOTICE: (c) 2018 Barr Group. All rights reserved.
 */

#include <arpa/inet.h> // inet_pton
#include <errno.h> // errno
#include <fcntl.h> // F_SETFL, O_NONBLOCK
#include <getopt.h> // getopt
#include <netinet/in.h> // sockaddr_in, INADDR_ANY
#include <sys/select.h>
#include <stdbool.h>
#include <stdio.h> // stdin, EOF
#include <stdlib.h> // EXIT_FAILURE
#include <string.h> // strlen
#include <strings.h> // strerror
#include <sys/socket.h> // connect
#include <sys/time.h> // timevalue
#include <unistd.h> // close

#include "cli_lib.h"

int main(int argc, char** argv)
{
    setbuf(stdout, NULL);
    extern char* optarg;
    
    char* p_serv_ip      = NULL;
    char* p_serv_port    = NULL;
    char* p_infix_string = NULL;
    int   flags          = 0;
    int   opt;
    do
    {
        opt = getopt(argc, argv, "i:p:e:");
        switch (opt)
        {
            case 'i':
                flags++;
                p_serv_ip = optarg;
            break;
            case 'p':
                flags++;
                p_serv_port = optarg;
            break;
            case 'e':
                p_infix_string = optarg;
            default:
            break;
        }
    } while (-1 != opt);

    if (2 > flags)
    {
        fprintf(stderr,
                "Usage: %s [-i SERV IP(v4)] [-p PORT] [-e INFIX STRING]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    int port_number = convert_port_number(p_serv_port);
    if (0 > port_number)
    {
        fprintf(stderr, "Port number must be in range [0-65535].\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in cli_addr = { 0 };
    cli_addr.sin_family         = AF_INET;
    cli_addr.sin_port           = htons(port_number);

    int err = inet_pton(AF_INET, p_serv_ip, &(cli_addr.sin_addr));
    if (1 > err)
    {
        fprintf(stderr, "A valid IPv4 address is needed.\n");
        return EXIT_FAILURE;
    }


    int client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > client_socket_fd)
    {
        fprintf(stderr,
                "Failed to create client socket. [%s]\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    socklen_t clilen = sizeof(cli_addr);

    err = connect(client_socket_fd, (struct sockaddr*)&cli_addr, clilen); 
    if (0 > err)
    {
        fprintf(stderr,
                "Unable to connect to server. [%s]\n",
                strerror(errno));
        close(client_socket_fd);
        return EXIT_FAILURE;
    }

    char buff[2] = { 0 };
    err = recv(client_socket_fd, buff, 1, 0);

    if ('0' != buff[0] && 1 != err)
    {
        fprintf(stderr,
                "Server handshake error. [%s]\n",
                strerror(errno));
        close(client_socket_fd);
        return EXIT_FAILURE;
    }

    if (NULL != p_infix_string)
    {
        if (MAX_BUFFER_SIZE < strlen(p_infix_string))
        {
            fprintf(stderr,
                    "Infix string is over 100 characters long.\n");
            return EXIT_FAILURE;
        }
        errno = 0;
        char* p_postfix = "3 2 -"; // Placeholder
        if (EINVAL == errno || NULL == p_postfix)
        {
            fprintf(stderr, "Error converting provided string.\n");
        }
        bool success = send_postfix(p_postfix, client_socket_fd);

        //free(p_postfix);
        if (false == success)
        {
            fprintf(stderr,
                    "An error occured while sending the equation to the server.\n");
            close(client_socket_fd);
            return EXIT_FAILURE;
        }
    }
    else
    {
        bool b_should_loop = true;

        char input_buffer[MAX_BUFFER_SIZE + 1] = { 0 };
        while (b_should_loop)
        {
            printf("Enter your math equation in Infix Notation or exit to");
            printf(" quit:\n");
            fgets(input_buffer, MAX_BUFFER_SIZE, stdin);
            purge_buffer();
            if (check_for_exit((char*)&input_buffer))
            {
                close(client_socket_fd);
                printf("Exiting.\n");
                return EXIT_SUCCESS;
            }
            char* p_postfix = "3 2 -"; // Placeholder
            bool success = send_postfix(p_postfix, client_socket_fd);
            //free(p_postfix);
            if (false == success)
            {
                fprintf(stderr,
                        "An error occured while sending the equation to the");
                fprintf(stderr, " server.\n");
                close(client_socket_fd);
                return EXIT_FAILURE;
            }
        }
    }
    close(client_socket_fd);
    return EXIT_SUCCESS;
} /* main */