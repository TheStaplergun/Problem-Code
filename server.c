/** @file server.c
 * 
 * @brief This program is a postfix notation solving server. It binds on the
 *        given port.
 *        The first argument provided should be the port number.
 * @par
 * COPYRIGHT NOTICE: (c) 2018 Barr Group. All rights reserved.
 */

#define _XOPEN_SOURCE 700 // sigaction
#include <errno.h> // errno
#include <getopt.h> // getopt
#include <netinet/in.h> // sockaddr_in, INADDR_ANY
#include <pthread.h> // pthread_mutex_init, pthread_mutex_lock
#include <signal.h> // sigaction, SIGINT
#include <stdbool.h>
#include <stdio.h> // stderr
#include <stdlib.h> // EXIT_FAILURE
#include <string.h> // strerror
#include <unistd.h> // close

#include "serv_lib.h"

serv_t g_serv = { 0 };

/**
 * @brief Signal interrupt handler function. Sets running to false and closes
 *        the server listener to unblock.
 */
void sig_interrupt_handler(int dummy)
{
    int count = 0;
    sem_getvalue(&(g_serv.client_count_sem), &count);
    //if (g_serv.max_connections == count)
    //{
        shutdown_server(&g_serv);
    //}
}

int main(int argc, char** argv)
{
    // Parse input arguments
    //
    if (2 > argc)
    {
        fprintf(stderr,
                "Usage: %s -p [0-65535](Port number) -n [2+](Thread count)\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    extern char* optarg;
    
    // Default of two
    //
    char* p_thread_count = "2";
    char* p_port_number  = NULL;

    int   opt;
    do
    {
        opt = getopt(argc, argv, "n:p:");
        switch (opt)
        {
            case 'n':
                p_thread_count = optarg;
            break;
            case 'p':
                p_port_number = optarg;
            default:
            break;
        }
    } while (-1 != opt);

    int port_number = convert_port_number(p_port_number);
    if (0 > port_number)
    {
        fprintf(stderr,
                "Usage: %s -p [0-65535](Port number) -n [2+](Thread count)\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    int thread_count       = convert_thread_count(p_thread_count);
    g_serv.max_connections = thread_count;

    // Create sig interrupt handler
    //
    struct sigaction handler = { 0 };
    handler.sa_handler       = &sig_interrupt_handler;

    int err = sigaction(SIGINT, &handler, NULL);
    if (0 > err)
    {
        printf("Error assigning sig handler [%s]\n", strerror(errno));
        return EXIT_FAILURE;
    }
    struct sockaddr_in serv_addr = { 0 };

    // Initialize socket
    //
    g_serv.serv_listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > g_serv.serv_listener_fd)
    {
        fprintf(stderr,
                "Failed to create listener socket. [%s]\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    int       optval = 1;
    socklen_t optlen = sizeof(optval);

    err = setsockopt(g_serv.serv_listener_fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
    if(0 > err) {
        fprintf(stderr,
                "Error setting socket options. [%s]\n",
                strerror(errno));
        close(g_serv.serv_listener_fd);
        return EXIT_FAILURE;
    }

    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(port_number);

    err = bind(g_serv.serv_listener_fd,
               (struct sockaddr*)&serv_addr,
               sizeof(serv_addr));
    if (0 > err)
    {
        fprintf(stderr,
                "Failed to bind on given port. [%s]\n",
                strerror(errno));
        return EXIT_FAILURE;
    }
    else
    {
        printf("Listener bound on port [%d]\n", port_number);
    }

    err = listen(g_serv.serv_listener_fd, 5);
    if (0 > err)
    {
        fprintf(stderr,
                "Error setting listening state. [%s]\n",
                strerror(errno));
        shutdown_server(&g_serv);
        return EXIT_FAILURE;
    }
    else
    {
        printf("Listener established.\n");
    }

    struct sockaddr_in cli_addr;
    int                client_fd = 0;

    socklen_t clilen = sizeof(cli_addr);

    // Set up server
    //
    init_server(&g_serv);

    while(g_serv.b_running)
    {
        //printf("Awaiting connection.\n");
        client_fd = accept(g_serv.serv_listener_fd,
                           (struct sockaddr*)&cli_addr,
                           &clilen);
        if (0 > client_fd)
        {
            fprintf(stderr,
                    "Error accepting connection. [%s]\n",
                    strerror(errno));
            continue;
        }


        err = sem_trywait(&(g_serv.client_count_sem));
        if (0 > err)
        {
            fprintf(stderr,
                    "Max connections reached. [%s]\n",
                    strerror(errno));
            notify_client_max_connections(client_fd);
            close(client_fd);
            client_fd = 0;
            continue;
        }

        printf("A client has connected.\n");

        char byte[2] = "0";
        err = send(client_fd, byte, 1, 0);

        // Set up client FD in global position and wake up a thread to grab it
        //
        pthread_mutex_lock(&(g_serv.new_connection_fd_lock));
        g_serv.new_connection_fd = client_fd;
        if (0 != g_serv.new_connection_fd)
        {
            pthread_cond_signal(&(g_serv.new_connection));
        }
        pthread_mutex_unlock(&(g_serv.new_connection_fd_lock));
    }
    
    shutdown_server(&g_serv);
} /* main */