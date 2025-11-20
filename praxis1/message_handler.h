#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

int find_crlfcrlf(const char *buf, int len);
void get_messages_and_send(int sockfd, char *buf, int *buf_len);

#endif
