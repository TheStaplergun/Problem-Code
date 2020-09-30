#define INVALID_PORT -1
#define MAX_BUFFER_SIZE 100

int  convert_port_number(char* p_string);
void purge_buffer();
int  check_for_exit(char *str);
bool send_postfix(char* p_postfix, int client_socket_fd);