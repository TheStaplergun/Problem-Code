#define _XOPEN_SOURCE 700
#include <ctype.h> // isdigit, isalpha
#include <errno.h>
#include <fcntl.h> // F_SETFL, O_NONBLOCK
#include <pthread.h>
#include <semaphore.h> // sem_post, sem_destroy, sem_trywait
#include <stdbool.h>
#include <stdio.h> // stderr, EOF, NULL
#include <stdlib.h> // strtol
#include <string.h> // strerror
#include <sys/types.h>
#include <sys/socket.h> // MSG_PEEK
#include <unistd.h> // close

#include "serv_lib.h"

#define PURGE_BUFFER_SIZE 256

/**
 * @brief Evaluate given character to see if it is a valid operator.
 *        One of (* = - / %)
 * @param[in] c A character to evaluate.
 * @return True if valid operator.
 *         False if not one of the valid operators.
 */

bool is_operator(char c)
{
    if ('*' == c ||
        '+' == c ||
        '-' == c ||
        '/' == c ||
        '%' == c)
    {
        return true;
    }
    return false;
}

/**
 * @brief Attempt to convert a string to a port number.
 * @param[in] p_string A pointer to a string containing the port number to
 *                     convert.
 * @return A valid port number between 0 and 65535.
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
}

int convert_thread_count(char* p_string)
{
    if (NULL == p_string)
    {
        fprintf(stderr, "No string to get thread count from.\n");
        return INVALID_PORT;
    }
    char* p_cursor        = p_string;
    char* p_cursor_memory = p_string;
    errno = 0;
    int val = strtol(p_cursor, &p_cursor_memory, 10);
    if (0 != errno)
    {
        fprintf(stderr, "[%s]\n", p_string);
        fprintf(stderr,
                "Conversion failed for given thread count. [%s]\n",
                strerror(errno));
        return MIN_THREADS;
    }

    if (MIN_THREADS > val)
    {
        printf("Thread count cannot be below 2. Setting thread count to 2.");
    }
    return (MIN_THREADS > val) ? MIN_THREADS : val;
} /* convert_thread_count */

/**
 * @brief Convert invalid characters before processing/printing to screen.
 * @param[in] p_string A pointer to a string to sanitize
 */
void sanitize_input_string(char* p_string)
{
    if (NULL == p_string)
    {
        fprintf(stderr, "No string provided to sanitize.\n");
        return;
    }
    char* p_cursor = p_string; 
    while ('\0' != *p_cursor)
    {
        if ('\n' == *p_cursor)
        {
            *p_cursor = '\0';
            break;
        }
        else if (!isdigit(*p_cursor) &&
                 !is_operator(*p_cursor) &&
                 '.' != *p_cursor)
        {
            *p_cursor = ' ';
        }
        p_cursor++;
    }
} /* sanitize_input_string */

/**
 * @brief Purges excess pending socket data if incoming message is longer than
 *        100 characters.
 * @param[in] socket_fd A File Descriptor for the socket to purge.
 */
void purge_socket(int socket_fd)
{
    char purge_buffer[PURGE_BUFFER_SIZE + 1] = { 0 };
    int  bytes_read                          = 0;

    // Set socket to nonblocking so functionality can continue without blocking
    // after purge.
    //
    int flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, (flags | O_NONBLOCK));
    do
    {
        bytes_read = recv(socket_fd, &purge_buffer, PURGE_BUFFER_SIZE + 1, 0);
    } while (PURGE_BUFFER_SIZE < bytes_read);
    fcntl(socket_fd, F_SETFL, (flags & ~O_NONBLOCK));
}

/**
 * @brief Reads provided client socket up to 100 characters, and purges the
 *        remaining data on the socket.
 * @param[in] client_fd The client's socket File Descriptor
 *            p_buffer A pointer to the a buffer to store the received message
 *                     from the connected client.
 * @return SOCK_READ_SUCCESS if successful read.
 *         SOCK_READ_ERROR if problem during receive. (Closes client socket)
 *         SOCK_CLIENT_DISCONNECT if the client has disconnected.
 *         SOCK_SEND_ERROR if the client sent too many characters and an error
 *             happens during notification to client (Send)
 */
int read_from_client(int client_fd, char* p_buffer)
{
    int status = SOCK_READ_SUCCESS;

    int pending_socket_buffer_length;
    int bytes_read;

    pending_socket_buffer_length = recv(client_fd,
                                        p_buffer,
                                        MAX_BUFFER_SIZE + 1,
                                        MSG_PEEK);
    // Truncate last byte to guarantee null terminator.
    //
    p_buffer[MAX_BUFFER_SIZE] = '\0';
    bytes_read = recv(client_fd, p_buffer, MAX_BUFFER_SIZE, 0);
    if (MAX_BUFFER_SIZE < pending_socket_buffer_length)
    {
        fprintf(stderr, "Data received exceeds 100 character limit.\n");
        char* p_message_too_long = 
        "Received message longer than 100 characters. Flushing excess.\n";
        int err = send(client_fd,
                       p_message_too_long,
                       strnlen(p_message_too_long, MAX_BUFFER_SIZE),
                       0);
        if (strnlen(p_message_too_long, MAX_BUFFER_SIZE) > err)
        {
            fprintf(stderr,
                    "Error sending message to client. [%s]\n",
                    strerror(errno));
            return SOCK_SEND_ERROR;
        }
        purge_socket(client_fd);
    }

    if (0 > bytes_read)
    {
        fprintf(stderr,
                "Error reading from socket. [%s]\n",
                strerror(errno));
        status = SOCK_READ_ERROR;
    }
    else if (0 == bytes_read)
    {
        printf("Client has disconnected.\n");
        status = SOCK_CLIENT_DISCONNECT;
    }
    else
    {
        sanitize_input_string(p_buffer);
        printf("Server received message: [%s]\n", p_buffer);
    }
    return status;
}

/**
 * @brief Handles a given client socket file descriptor. Processes an equation
 *        received on the given client file descriptor.
 * @param[in] client_fd The client's socket File Descriptor
 * @return True if the client is still connected.
 *         False if the client has disconnected.
 */
bool handle_client(int client_fd)
{
    int  err;
    bool is_connected = true;

    char* p_buffer = calloc(MAX_BUFFER_SIZE + 1, sizeof(char));
    if (NULL == p_buffer)
    {
        fprintf(stderr,
                "Error allocating memory for socket buffer. [%s]\n",
                strerror(errno));
        char* p_error_message = "Server error. Disconnecting client.\n";
        err = send(client_fd,
                   p_error_message,
                   strnlen(p_error_message, MAX_BUFFER_SIZE),
                   0);
        if (strnlen(p_error_message, MAX_BUFFER_SIZE) > err)
        {
            fprintf(stderr,
                    "Error sending message to client. [%s]\n",
                    strerror(errno));
        }

        return false;
    }

    err = read_from_client(client_fd, p_buffer);
    if (0 > err)
    {
        char* p_error_message = "Server error. Disconnecting client.\n";
        err = send(client_fd,
                   p_error_message,
                   strnlen(p_error_message, MAX_BUFFER_SIZE),
                   0);
        free(p_buffer);
        return false;
    }

    errno = 0;
    float answer = 1.0; // Placeholder for simplification
    if (EINVAL == errno)
    {
        fprintf(stderr, "Invalid equation given or an error has ");
        fprintf(stderr, "occurred.\nNotifying client.\n");
        char* p_err_message = 
                "An error occurred processing the given equation.";
        err = send(client_fd,
                   p_err_message,
                   strnlen(p_err_message, MAX_BUFFER_SIZE),
                   0);
        if (strnlen(p_err_message, MAX_BUFFER_SIZE) > err)
        {
            fprintf(stderr,
                    "Error sending message to client. [%s]\n",
                    strerror(errno));
            is_connected = false;
        }
    }
    else
    {
        char response[MAX_BUFFER_SIZE] = { 0 };
        printf("The answer to the equation sent by the client is [%f]\n",
                answer);
        char* p_format_string =
                "The answer to the given equation is [%f]";
        sprintf(response, p_format_string, answer);
        err = send(client_fd,
                   response,
                   strnlen(response, MAX_BUFFER_SIZE),
                   0);
        if (strnlen(response, MAX_BUFFER_SIZE) > err)
        {
            fprintf(stderr,
                    "Error sending message to client. [%s]\n",
                    strerror(errno));
            is_connected = false;
        }
    }
    free(p_buffer);
    return is_connected;
} /* handle_client */

/**
 * @brief Notifies a given client that it is unable to accept the connection
 *        and disconnects them.
 * @param[in] client_fd The client's socket File Descriptor
 */
void notify_client_max_connections(int client_fd)
{
    char* message = "Unable to accept connection. Try again later.\n";
    send(client_fd,
         message,
         strnlen(message, MAX_BUFFER_SIZE),
         0);
} /* notify_and_disconnect_client */

/**
 * @brief Notifies a given client that it is unable to accept the connection
 *        and disconnects them.
 * @param[in] client_fd The client's socket File Descriptor
 * @return NULL on thread exit
 */
void* thread_handler(void* args)
{
    serv_t* p_serv = (serv_t*)args;
    bool    thread_client_connected;
    int     thread_client_fd;
    while(p_serv->b_running)
    {
        pthread_mutex_lock(&(p_serv->new_connection_fd_lock));
        while (0 == p_serv->new_connection_fd && p_serv->b_running)
        {
            pthread_cond_wait(&(p_serv->new_connection),
                              &(p_serv->new_connection_fd_lock));
        }
        thread_client_fd = p_serv->new_connection_fd;
        p_serv->new_connection_fd = 0;
        pthread_mutex_unlock(&(p_serv->new_connection_fd_lock));

        // In the case of a pthread cond broadcast for exiting the server.
        //
        if (0 == thread_client_fd)
        {
            continue;
        }
        
        thread_client_connected = true;
        while (thread_client_connected)
        {
            thread_client_connected = handle_client(thread_client_fd);
        }
        close(thread_client_fd);
        thread_client_fd = 0;
        sem_post(&(p_serv->client_count_sem));
    }
    return NULL;
} /* thread_handler */

/**
 * @brief Shuts down a given server within a serv_t object.
 * @param[in] p_serv A pointer to an initialized serv_t struct.
 */
void shutdown_server(serv_t* p_serv)
{
    int err;

    // Prevents double shutdown
    //
    if (false == p_serv->b_running)
    {
        return;
    }

    p_serv->b_running = false;
    pthread_cond_broadcast(&(p_serv->new_connection));

    for(int i = 0; i < p_serv->max_connections; i++)
    {
        err = pthread_join(p_serv->p_thread_ids[i], NULL);
        if (0 > err)
        {
            fprintf(stderr, "Error joining thread. [%s]\n", strerror(err));
        }
    }

    int test = 0;
    sem_getvalue(&(p_serv->client_count_sem), &test);
    printf("Sem count client [%d]\n", test);
    err = sem_destroy(&(p_serv->client_count_sem));
    if (0 > err)
    {
        fprintf(stderr,
                "Error destroying client count semaphore. [%s]\n",
                strerror(errno));
    }

    err = pthread_mutex_destroy(&(p_serv->new_connection_fd_lock));
    if (0 > err)
    {
        fprintf(stderr,
                "Error destroying new connection mutex. [%s]\n",
                strerror(errno));
    }

    err = pthread_cond_destroy(&(p_serv->new_connection));
    if (0 > err)
    {
        fprintf(stderr,
                "Error destroying new connection condition var. [%s]\n",
                strerror(err));
    }
    free(p_serv->p_thread_ids);
} /* shutdown_server */

/**
 * @brief Initializes threads, mutex, and semaphores for server and stores
 *        them in the given serv_t struct.
 * @param[in] p_serv A pointer to an initialized serv_t struct.
 * @return SERV_INIT_SUCCESS if the server started successfully.
 *         SERV_INIT_FAILURE if the server components fail to start properly.
 */
int init_server(serv_t* p_serv)
{
    int err;
    p_serv->b_running = true;
    p_serv->p_thread_ids = calloc(p_serv->max_connections, sizeof(pthread_t));

    err = pthread_mutex_init(&(p_serv->new_connection_fd_lock), NULL);
    if (0 > err)
    {
        fprintf(stderr,
                "Unable to initiate new connection mutex. [%s]\n",
                strerror(errno));
        shutdown_server(p_serv);
        return SERV_INIT_FAILURE;
    }

    err = pthread_cond_init(&(p_serv->new_connection), NULL);
    if (0 > err)
    {
        fprintf(stderr,
                "Unable to initiate thread condition. [%s]\n",
                strerror(errno));
        shutdown_server(p_serv);
        return SERV_INIT_FAILURE;
    }

    for(int i = 0; i < p_serv->max_connections; i++)
    {
        err = pthread_create(&(p_serv->p_thread_ids[i]),
                             NULL,
                             &thread_handler,
                             p_serv);
        if (0 > err)
        {
            fprintf(stderr,
                    "Thread unable to be created. [%s]\n",
                    strerror(err));
            shutdown_server(p_serv);
            return SERV_INIT_FAILURE;
        }
    }

    err = sem_init(&(p_serv->client_count_sem), 0, p_serv->max_connections);
    if (0 > err)
    {
        fprintf(stderr,
                "Unable to initiate client count semaphore. [%s]\n",
                strerror(errno));
        shutdown_server(p_serv);
        return SERV_INIT_FAILURE;
    }

    return SERV_INIT_SUCCESS;
} /* init_server */