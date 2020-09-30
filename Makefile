all: PostfixObjs

PostfixObjs: PostfixServ PostfixClient
CFLAGS=-std=c11 -Wall -Werror -Wpedantic
CLIENT_POSTFIX_FLAGS=-lm
SERV_POSTFIX_FLAGS=-lm -pthread

#SERV_COMPONENTS=../../Stack/hochheimer/my_stack.c
#SERV_COMPONENTS+=../../Postfix_Evaluator/hochheimer/postfix_eval.c
SERV_COMPONENTS+=serv_lib.c server.c

#CLI_COMPONENTS=../../Stack/hochheimer/my_stack.c
#CLI_COMPONENTS+=../../Postfix_Converter/hochheimer/postfix_convert.c
CLI_COMPONENTS+=cli_lib.c client.c

PostfixServ:
	gcc $(CFLAGS) $(SERV_COMPONENTS) -o postfix_server $(SERV_POSTFIX_FLAGS)

PostfixClient:
	gcc $(CFLAGS) $(CLI_COMPONENTS) -o postfix_client $(CLIENT_POSTFIX_FLAGS)

clean:
	rm postfix_client postfix_server