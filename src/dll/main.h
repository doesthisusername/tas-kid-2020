#pragma once

#include <stdio.h>
#include <windows.h>
#include "types.h"

#define SHOULD_LOG 1
#if SHOULD_LOG
#define LOG(x, ...) fprintf(outf, x, ##__VA_ARGS__); fflush(outf)
#else
#define LOG(x, ...)
#endif

extern FILE* outf;
extern void* hat_base;
extern void* hat_end;
extern void* xinput_base;
extern SOCKET ws;
extern SOCKET wc;

extern u32 socket_good;
void connect_socket();

// Returns non-zero if reconnecting in the background
u32 warn_disconnect();

// Remember to free return value, and check for NULL, meaning failure
char* recv_msg();

// Remember to check return value, 0 being failure
u32 send_msg(char* msg, u32 len);

char* append_mem(char* str, const char* append, size_t* size, size_t* max);

void main_thread(FILE* logf);
