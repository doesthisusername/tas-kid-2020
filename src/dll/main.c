#include <windows.h>
#include "hook.h"
#include "offsets.h"
#include "main.h"

#define PORT 8079

FILE* outf;
void* hat_base;
void* hat_end;
void* xinput_base;
SOCKET ws;
SOCKET wc;

u32 socket_good;

void connect_socket() {
	ws = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(ws, SOL_SOCKET, TCP_NODELAY, (const char*)&(int){1}, sizeof(int));
	setsockopt(ws, SOL_SOCKET, SO_DONTLINGER, (const char*)&(int){0}, sizeof(int));

	struct sockaddr_in addr;
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(PORT);

	(void)bind(ws, (SOCKADDR*)&addr, sizeof(addr));

	listen(ws, 1);
	wc = accept(ws, NULL, NULL);
	socket_good = 1;
	LOG("Connected socket\n");
}

void reconnect_socket() {
	listen(ws, 1);
	wc = accept(ws, NULL, NULL);
	socket_good = 1;
	LOG("Reconnected socket\n");
}

u32 warn_disconnect() {
	closesocket(ws);
	closesocket(wc);

	const s32 result = MessageBox(NULL, "Disconnected from the server. Click Retry to attempt to reconnect immediately, or Cancel to reconnect in the background.", "Disconnection", MB_RETRYCANCEL | MB_ICONERROR);
	if(result == IDRETRY) {
		reconnect_socket();
		return 0;
	}
	else {
		CreateThread(NULL, 0, (unsigned long (*)(void*))reconnect_socket, NULL, 0, NULL);
		return 1;
	}
}

char* recv_msg() {
	char len_msg[5];

	s32 n = recv(wc, len_msg, 4, 0);
	if(n == 0) {
		if(warn_disconnect()) {
			// Not going to reconnect immediately, so wait
			return NULL;
		}
		else {
			// Try again
			return recv_msg();
		}
	}

	len_msg[4] = '\0';
	s32 len = atoi(len_msg);
	LOG("Received message of length %d\n", len);

	char* payload = malloc(len + 1);

	if(len > 0) {
		n = recv(wc, payload, len, 0);
		if(n == 0) {
			free(payload);
			warn_disconnect();
			return NULL;
		}
	}

	payload[len] = '\0';
	LOG("Received message %s\n", payload);
	
	return payload;
}

u32 send_msg(char* msg, u32 len) {
	LOG("Sending message %s\n", msg);
	const s32 n = send(wc, msg, len, 0);

	if(n == -1) {
		if(warn_disconnect()) {
			return 0;
		}
		else {
			return send_msg(msg, len);
		}
	}
	else {
		return 1;
	}
}

char* append_mem(char* str, const char* append, size_t* size, size_t* max) {
	const size_t alen = strlen(append);

	if(*size + alen >= *max) {
		*max *= 2;
		str = realloc(str, *max);
	}

	memcpy(str + *size, append, alen);
	*size += alen;

	return str;
}

void main_thread(FILE* log) {
	outf = log;
	LOG("Started main thread\n");

	// Get base addresses of needed modules
	hat_base = GetModuleHandle("HatinTimeGame.exe");
	LOG("HatinTimeGame.exe's base address is %p\n", hat_base);
	xinput_base = GetModuleHandle("XInput1_3.dll");
	LOG("XInput1_3.dll's base address is %p\n", xinput_base);

	// Iterate through the main executable's memory regions to find the end
	MEMORY_BASIC_INFORMATION info;
	info.RegionSize = 0;
	void* base = hat_base;
	do {
		base = (u8*)base + info.RegionSize;
		VirtualQuery(base, &info, sizeof(info));
	} 
	while(info.AllocationBase == hat_base);
	hat_end = base;
	
	LOG("HatinTimeGame.exe's end address is %p\n", hat_end);

	// Initialize version-describing structures
	const u32 timestamp = get_exe_timestamp(hat_base);
	if(!init_ver(timestamp)) {
		MessageBox(NULL, "I'm going to crash now", "Unsupported A Hat in Time version!", MB_OK | MB_ICONERROR);
	}
	LOG("Initialized version structures with timestamp %u\n", timestamp);

	// Set benchmark
	//*(u32*)(hat_base + 0x10AF6EC) = 1;

	// Inline RNG
	//replace_all_u32(0x0BB38435, 0x00000001);
	//replace_all_u32(0x3619636B, 0x00000000);

	// Hook native delta
	void** native_delta = find_native_func("AHat_GameManager_BaseexecGetNativeRealTimeDelta");
	orig_native_delta = *native_delta;
	update_func_ptr(native_delta, new_native_delta);

	// Hook load screen
	void** create_load = find_native_func("UHat_GlobalDataInfoexecCreateLoadingScreen");
	orig_create_load = *create_load;
	update_func_ptr(create_load, new_create_load);

	// Hook tick function (delaying until game is initialized)
	void** tick_func;
	do {
		tick_func = resolve_path(&ver.uengine_tick);
		Sleep(50);
	}
	while(tick_func == NULL);
	orig_tick = *tick_func;
	update_func_ptr(tick_func, new_tick);

	orig_poll = hat_base + 0x9BAE70; // fix!!
	update_func_ptr((void**)(hat_base + 0xEB41F8), new_poll);

	// Hook XInputGetState
	orig_get_state = *ver.xinput_get_state;
	update_func_ptr(ver.xinput_get_state, new_get_state);

	// For KBM input. Doesn't work properly yet.
	//
	// Hook GetKeyState
	// orig_get_key_state = *ver.user_get_key_state;
	// update_func_ptr(ver.user_get_key_state, new_get_key_state);

	// // Hook InputKey
	// orig_input_key = *ver.input_key;
	// update_func_ptr(ver.input_key, new_input_key);

	// // Hook GetDeviceData (mouse)
	// struct IDirectInputDevice8AVtbl* vtbl = (**ver.dinput8_mouse).lpVtbl;
	// orig_get_device_data = vtbl->GetDeviceData;
	// update_func_ptr((void**)&vtbl->GetDeviceData, new_get_device_data);

	// Hook rand
	orig_rand = *ver.msvcr_rand;
	update_func_ptr(ver.msvcr_rand, new_rand);

	// Hook uc rand
	orig_urand = ver.byte_op_table[OP_RAND];
	update_func_ptr(&ver.byte_op_table[OP_RAND], new_urand);

	// Hook uc frand
	orig_frand = ver.byte_op_table[OP_FRAND];
	update_func_ptr(&ver.byte_op_table[OP_FRAND], new_frand);

	// Init socket
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
	LOG("Started WSA\n");

	connect_socket();
}
