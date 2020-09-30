#include <semaphore.h> // sem_t

#define INVALID_PORT -1
#define MAX_BUFFER_SIZE 100
#define SOCK_READ_SUCCESS 0
#define SOCK_READ_ERROR -1
#define SERV_INIT_SUCCESS 0
#define SERV_INIT_FAILURE -1
#define SOCK_CLIENT_DISCONNECT -2
#define SOCK_SEND_ERROR -3
#define MIN_THREADS 2

typedef struct serv_t {
    bool            b_running;
    int             max_connections;
    int             serv_listener_fd;
    sem_t           client_count_sem;
    pthread_mutex_t new_connection_fd_lock;
    pthread_cond_t  new_connection;
    int             new_connection_fd;
    pthread_cond_t  connection_accepted;
    pthread_t*      p_thread_ids;
} serv_t;

int  convert_port_number(char* p_string);
int  convert_thread_count(char* p_string);
bool handle_client(int client_fd);
void notify_client_max_connections(int client_fd);
void shutdown_server(serv_t* p_serv);
int  init_server(serv_t* p_serv);