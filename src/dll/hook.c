#include <string.h>
#include <math.h>
#include <windows.h>
#include <xinput.h>
#include "types.h"
#include "main.h"
#include "offsets.h"
#include "tas.h"
#include "hook.h"

void(*orig_native_delta)(void* unk_a, struct FFrame* ctx, float* out);
void(*orig_create_load)(void* unk_a, struct FFrame* ctx);
u32(*orig_is_dlc_installed)(void* this, int app_id);
void(*orig_tick)(void* delta, float unk_b, float unk_a);
HRESULT(*orig_poll)(void* unk_a);
u32(*orig_get_state)(u32 player_id, XINPUT_STATE* state);
u16(*orig_get_key_state)(s32 key_id);
u32(*orig_input_key)(void* this, int param_1, struct FName key, u8 event, float param_4, u32 param_5);
HRESULT(*orig_get_device_data)(IDirectInputDevice8* self, DWORD object_data, DIDEVICEOBJECTDATA* rgdod, DWORD* in_out, DWORD flags);
s32(*orig_rand)();
void(*orig_urand)(struct UObject* this, struct FFrame* ctx, s32* out);
void(*orig_frand)(struct UObject* this, struct FFrame* ctx, float* out);


float delta_time = 1 / 60.0f;
float orig_fps = 60.0f;
float fps = -1.0f;
u32 frame_counter = 0;
u32 started_this_frame = 0;

struct rand_info sys_rand;
struct rand_info urand;
struct rand_info frand;

u32 did_u = 0;
u32 did_f = 0;

u32 stepping = 0;

struct key_queue key_queue[KEY_QUEUE_N];
struct mouse mouse_movement;

void** find_native_func(const char* name) {
	// This _may_ be faster, as this allows us to use memcmp() instead
	const size_t name_len = strlen(name) + 1;
	
	// Find string
	char* name_ptr = NULL;
	for(char* i = hat_base; i < (char*)hat_end; i += 8) {
		if(memcmp(name, i, name_len) == 0) {
			LOG("Found native function name %s at %p\n", i, i);
			name_ptr = i;
			break;
		}
	}

	if(name_ptr == NULL) {
		LOG("Failed to find native function name %s!\n", name);
		return NULL;
	}

	// Find pointer to string
	void** ptr = NULL;
	for(void** i = hat_base; i < (void**)hat_end; i++) {
		if(*i == (void*)name_ptr) {
			LOG("Found native function pointer at %p\n", i + 1);
			ptr = i + 1;
			break;
		}
	}

	if(ptr == NULL) {
		LOG("Failed to find native function pointer for %s!\n", name);
		return NULL;
	}

	return ptr;
}

void update_func_ptr(void** old, void* new) {
	const void* original = *old; // For the print
	DWORD old_protect;

	VirtualProtect(old, sizeof(old), PAGE_READWRITE, &old_protect);
	*old = new;
	VirtualProtect(old, sizeof(old), old_protect, NULL);

	LOG("Updated function pointer at %p (%p -> %p)\n", old, original, *old);
}

void replace_all_u32(u32 old, u32 new) {
	DWORD old_protect;

	for(char* i = hat_base; i < (char*)hat_end - sizeof(u32); i++) {
		if(*(u32*)i == old) {
			VirtualProtect(i, sizeof(u32), PAGE_EXECUTE_READWRITE, &old_protect);
			*(u32*)i = new;
			VirtualProtect(i, sizeof(u32), old_protect, NULL);
		}
	}
}

void run_expr(struct FFrame* ctx, void* out) {
	const u8 token = *(ctx->pc);
	ctx->pc++;

	((void (*)(void*, struct FFrame*, void*))ver.byte_op_table[token])(ctx->unk_ptr, ctx, out);

	ctx->pc++;
}

void send_new_frame(float delta) {
	char to_send[32];
	snprintf(to_send, sizeof(to_send), "new-frame %f", delta);
	send_msg(to_send, strlen(to_send));
	char* msg = recv_msg();

	u32 did_sys = 0;
	did_u = 0;
	did_f = 0;

	fps = -1.0f;

	char* tok = msg;
	char* next;
	while(1) {
		// Separate at spaces
		next = strchr(tok, ' ');
		if(next != NULL) {
			*next = '\0';
		}

		// Check for commands, which are followed by their value (example DELTA0.0167)
		if(strncmp(tok, "DELTA", sizeof("DELTA") - 1) == 0) {
			delta_time = atof(tok + sizeof("DELTA") - 1);
		}
		else if(strncmp(tok, "FPS", sizeof("FPS") - 1) == 0) {
			fps = atof(tok + sizeof("FPS") - 1);
		}
		// The value here is a comma-separated list (example SYSTEMRAND23,0,32,55)
		else if(strncmp(tok, "SYSTEMRAND", sizeof("SYSTEMRAND") - 1) == 0) {
			char* list = tok + sizeof("SYSTEMRAND") - 1;
			parse_rand(&sys_rand, list);
			did_sys = 1;
		}
		else if(strncmp(tok, "URAND", sizeof("URAND") - 1) == 0) {
			char* list = tok + sizeof("URAND") - 1;
			parse_rand(&urand, list);
			did_u = 1;
		}
		else if(strncmp(tok, "FRAND", sizeof("FRAND") - 1) == 0) {
			char* list = tok + sizeof("FRAND") - 1;
			parse_rand(&frand, list);
			did_f = 1;
		}
		// The value here is the mode to switch to: none, record, or replay
		else if(strncmp(tok, "MODE", sizeof("MODE") - 1) == 0) {
			const char* mode_name = tok + sizeof("MODE") - 1;
			if(strcmp(mode_name, "none") == 0) {
				mode = TAS_NONE;
			}
			else if(strcmp(mode_name, "record") == 0) {
				mode = TAS_RECORD;
			}
			else if(strcmp(mode_name, "replay") == 0) {
				mode = TAS_REPLAY;
			}
		}
		// The value here is a boolean in the form of on, off
		else if(strncmp(tok, "STEP", sizeof("STEP") - 1) == 0) {
			const char* step_type = tok + sizeof("STEP") - 1;
			if(strcmp(step_type, "on") == 0) {
				stepping = 1;
			}
			else if(strcmp(step_type, "off") == 0) {
				stepping = 0;
			}
		}

		tok = next + 1;
		if(next == NULL) {
			break;
		}
	}

	// Reset to default if unspecified
	if(!did_sys) {
		parse_rand(&sys_rand, "0");
	}
}

void new_native_delta(void* unk_a, struct FFrame* ctx, float* out) {
	orig_native_delta(unk_a, ctx, out);
	*out = delta_time;
}

void new_create_load(void* unk_a, struct FFrame* ctx) {
	static u32 first_run = 1;

	if(first_run) {
		orig_fps = *(float*)resolve_path(&ver.fps_path);
		first_run = 0;
	}

	u8* orig_pc = ctx->pc;
	s32 type;
	run_expr(ctx, &type);
	ctx->pc = orig_pc; // Assume that the above expression doesn't have any side-effects

	LOG("[%04d] Creating load screen of type %d\n", frame_counter, type);
	orig_create_load(unk_a, ctx);
	
	if(type == 1) {
		if(socket_good) {
			frame_counter = 0;
			
			send_msg("begin-load", sizeof("begin-load") - 1);
			char* msg = recv_msg();

			if(strncmp(msg, "replay", sizeof("replay") - 1) == 0) {
				mode = TAS_REPLAY;
				*ver.particle_rand = 0;
				orig_fps = *(float*)resolve_path(&ver.fps_path);
				LOG("Starting replay\n");
			}
			else if(strncmp(msg, "record", sizeof("record") - 1) == 0) {
				mode = TAS_RECORD;
				*ver.particle_rand = 0;
				LOG("Starting record\n");
			}
			else {
				mode = TAS_NONE;
				*(float*)resolve_path(&ver.fps_path) = orig_fps;
				LOG("Starting nothing\n");
			}

			free(msg);

			if(mode != TAS_NONE) {
				send_new_frame(0.0f);
			}
		}
	}
}

// WIP (KBM input)
u32 new_input_key(void* this, int param_1, struct FName key, u8 event, float param_4, u32 param_5) {
	return orig_input_key(this, param_1, key, event, param_4, param_5);

	static int x = 1;
	if(x) {
		key.name_index = 0x1d9a;
	}
	x ^= 1;
	const u32 r = orig_input_key(this, param_1, key, event, param_4, param_5);
	LOG("InputKey: this=%p, param_1=%08x, key.id=%08x, key.name_index=%08x, event=%02hhx, param_4=%f, param_5=%08x -> %08x\n", this, param_1, key.id, key.name_index, event, param_4, param_5, r);
	return r;	
}

static int rshift_pressed() {
	static int last_rshift = 0;

	int rshift_down = !!(GetAsyncKeyState(VK_RSHIFT) & 0x8000);
	const int rshift_pressed = rshift_down && !last_rshift;

	last_rshift = rshift_down;
	return rshift_pressed;
}

void new_tick(void* strct, float delta, float unk_f) {
	if(stepping || rshift_pressed()) {
		stepping = 1;

		while(1) {
			if(rshift_pressed()) {
				break;
			}
			else if(GetAsyncKeyState(VK_LCONTROL)) {
				stepping = 0;
				break;
			}
		}
	}

	delta_time = delta;
	*ver.particle_rand = (frame_counter * 1024) % 32768;

	if(mode != TAS_NONE && socket_good) {
		send_new_frame(delta);
	}

	frame_counter++;

	if(fps == -1.0f) {
		fps = 1 / delta_time;
	}

	if(mode == TAS_REPLAY) {
		*(float*)resolve_path(&ver.fps_path) = fps;
	}
	
	LOG("[%04d] mode %d -> %X [%f] (%f)\n", frame_counter, mode, *(u32*)&delta_time, delta, unk_f);

	// WIP KBM input
	// if(mode == TAS_REPLAY) {
	// 	for(u32 i = 0; i < KEY_QUEUE_N; i++) {
	// 		LOG("%02X: %zu/%zu\n", i, key_queue[i].val_i, key_queue[i].val_n);
	// 	}
	// 	reset_key_queue(key_queue);
	// 	recv_key_states(key_queue);

	// 	memset(&mouse_movement, 0x00, sizeof(mouse_movement));
	// 	recv_mouse_movement(&mouse_movement);
	// }

	// Is this right???
	const float sort_of_fps = delta_time > 1 / 30.0f ? delta_time > 1.0f ? 1.0f : 0.0f : 1 / delta_time;
	orig_tick(strct, delta_time, sort_of_fps);

	// WIP KBM input
	// if(mode == TAS_RECORD) {
	// 	send_key_states(key_queue);
	// 	reset_key_queue(key_queue);

	// 	send_mouse_movement(&mouse_movement);
	// 	memset(&mouse_movement, 0x00, sizeof(mouse_movement));
	// }
}

HRESULT new_poll(void* unk_a) {
	//LOG("this var is %08X\n", *(u32*)((u8*)hat_base + 0x10b9758));
	//LOG("polling yay, unk_a is %p, and *(unk_a + 0x5C) is %016lX, and *(unk_a + 0xD0) is %016lX\n", unk_a, *(u64*)((u8*)unk_a + 0x5C), *(u64*)((u8*)unk_a + 0xD0));
	return orig_poll(unk_a);
	//LOG("e %08X returning to %p\n", *(u32*)((u8*)unk_a + 0x5D8), __builtin_return_address(0));
}

u32 new_get_state(u32 player_id, XINPUT_STATE* state) {
	const u32 result = orig_get_state(player_id, state);

	if(socket_good && mode == TAS_REPLAY) {
		char to_send[32];
		snprintf(to_send, sizeof(to_send), "input-what x360 %u", player_id);
		send_msg(to_send, strlen(to_send));
		if(!socket_good) {
			return result;
		}

		char* msg = recv_msg();
		if(!socket_good) {
			return result;
		}

		memset(&state->Gamepad, 0x00, sizeof(state->Gamepad));

		char* tok = msg;
		char* next;
		while(1) {
			next = strchr(tok, ' ');
			if(next != NULL) {
				*next = '\0';
			}

			// Deserves refactoring
			const size_t prefix_size = sizeof("BTN_") - 1;
			if(strncmp(tok, "BTN_", prefix_size) == 0) {
				state->Gamepad.wButtons |= x360_btn_to_num(tok + prefix_size);
			}
			else {
				const size_t axis_size = sizeof("AXIS_XX") - 1;
				if(strncmp(tok, "AXIS_LS", axis_size) == 0) {
					sscanf_s(tok + axis_size, "%hd;%hd", &state->Gamepad.sThumbLX, &state->Gamepad.sThumbLY);
				}
				else if(strncmp(tok, "AXIS_RS", axis_size) == 0) {
					sscanf_s(tok + axis_size, "%hd;%hd", &state->Gamepad.sThumbRX, &state->Gamepad.sThumbRY);
				}
				else {
					const size_t trigger_size = sizeof("TRIGGER_XT") - 1;
					if(strncmp(tok, "TRIGGER_LT", trigger_size) == 0) {
						state->Gamepad.bLeftTrigger = 0xFF;
					}
					else if(strncmp(tok, "TRIGGER_RT", trigger_size) == 0) {
						state->Gamepad.bRightTrigger = 0xFF;
					}
				}
			}

			tok = next + 1;
			if(next == NULL) {
				break;
			}
		}

		free(msg);
	}
	else if(socket_good && mode == TAS_RECORD) {
		size_t max = 0x100;
		char* to_send = realloc(NULL, max); // Should be sufficient, but just in case
		size_t size = snprintf(to_send, max, "input-this x360 %u", player_id);

#define CHECKBTN(x) if(state->Gamepad.wButtons & XINPUT_GAMEPAD_ ## x) to_send = append_mem(to_send, " BTN_" # x, &size, &max)
		CHECKBTN(A);
		CHECKBTN(B);
		CHECKBTN(X);
		CHECKBTN(Y);
		CHECKBTN(LEFT_SHOULDER);
		CHECKBTN(RIGHT_SHOULDER);
		CHECKBTN(START);
		CHECKBTN(BACK);
		CHECKBTN(LEFT_THUMB);
		CHECKBTN(RIGHT_THUMB);
		CHECKBTN(DPAD_UP);
		CHECKBTN(DPAD_RIGHT);
		CHECKBTN(DPAD_DOWN);
		CHECKBTN(DPAD_LEFT);
#undef CHECKBTN

		if(state->Gamepad.sThumbLX != 0 || state->Gamepad.sThumbLY != 0) {
			char stick[32];
			snprintf(stick, sizeof(stick), " AXIS_LS%hd;%hd", state->Gamepad.sThumbLX, state->Gamepad.sThumbLY);
			to_send = append_mem(to_send, stick, &size, &max);
		}
		if(state->Gamepad.sThumbRX != 0 || state->Gamepad.sThumbRY != 0) {
			char stick[32];
			snprintf(stick, sizeof(stick), " AXIS_RS%hd;%hd", state->Gamepad.sThumbRX, state->Gamepad.sThumbRY);
			to_send = append_mem(to_send, stick, &size, &max);
		}

		// TODO: revise threshold
		if(state->Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
			to_send = append_mem(to_send, " TRIGGER_LT", &size, &max);
		}
		if(state->Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
			to_send = append_mem(to_send, " TRIGGER_RT", &size, &max);
		}

		send_msg(to_send, size);
		free(to_send);

		// We can go too fast for the server at times, so block on a response
		char* block = recv_msg();
		if(!socket_good) {
			return result;
		}

		free(block);
	}

	return result;
}

static void ensure_key_queue_has_room(struct key_queue* queue, s32 key) {
	struct key_queue* this = &queue[key];

	if(this->val_n == this->val_max) {
		this->val_max += 2;
		this->vals = realloc(this->vals, sizeof(*this->vals) * this->val_max);
	}
}

void reset_key_queue(struct key_queue* queue) {
	for(u32 i = 0; i < KEY_QUEUE_N; i++) {
		if(!queue[i].vals) {
			queue[i].vals = realloc(NULL, sizeof(*queue[i].vals) * 4);
			queue[i].val_max = 4;
		}

		queue[i].val_n = 0;
		queue[i].val_i = 0;
	}
}

void add_key_queue_key(struct key_queue* queue, s32 key, u16 val) {
	struct key_queue* this = &queue[key];

	ensure_key_queue_has_room(queue, key);
	this->vals[this->val_n] = val;
	(this->val_n)++;
}

void recv_key_states(struct key_queue* queue) {
	if(!socket_good) {
		return;
	}

	send_msg("input-what kb 0", sizeof("input-what kb 0"));
	if(!socket_good) {
		return;
	}

	char* msg = recv_msg();
	if(!socket_good) {
		return;
	}

	if(strlen(msg) == 0) {
		free(msg);
		return;
	}

	char* tok = msg;
	char* next;
	while(1) {
		next = strchr(tok, ' ');
		if(next != NULL) {
			*next = '\0';
		}

		s32 key;
		u16 val;
		sscanf_s(tok, "%X;%hX", &key, &val);
		//LOG("%s -> %02X;%04X\n", tok, key, val);
		add_key_queue_key(queue, key, val);

		tok = next + 1;
		if(next == NULL) {
			break;
		}
	}

	free(msg);
}

void send_key_states(struct key_queue* queue) {
	size_t max = 0x100;
	char* to_send = realloc(NULL, max);
	size_t size = snprintf(to_send, max, "input-this kb 0");

	for(u32 i = 0; i < KEY_QUEUE_N; i++) {
		struct key_queue* curr = &queue[i];

		for(u32 j = 0; j < curr->val_n; j++) {
			char str[32];
			snprintf(str, sizeof(str), " %02X;%04X", i, curr->vals[j]);
			to_send = append_mem(to_send, str, &size, &max);
		}
	}

	send_msg(to_send, size);
	free(to_send);
	if(!socket_good) {
		return;
	}

	// We can go too fast for the server at times, so block on a response
	char* block = recv_msg();
	if(!socket_good) {
		return;
	}

	free(block);
}

// WIP (keyboard input)
u16 new_get_key_state(s32 key_id) {
	return orig_get_key_state(key_id);

	switch(mode) {
		case TAS_NONE: {
			return orig_get_key_state(key_id);
		}
		case TAS_RECORD: {
			const u16 val = orig_get_key_state(key_id);
			add_key_queue_key(key_queue, key_id, val);
			//LOG("Added %02X;%04X\n", key_id, val);
			return val;
		}
		case TAS_REPLAY: {
			return key_queue[key_id].vals[key_queue[key_id].val_i++];
		}
		default: __builtin_unreachable();
	}
}

static void dump_rgdod(const DIDEVICEOBJECTDATA* rgdod) {
	LOG(
		"struct DIDEVICEOBJECTDATA {\n"
		"\tDWORD dwOfs = %lu;\n"
		"\tDWORD dwData = %lu;\n"
		"\tDWORD dwTimeStamp = %lu;\n"
		"\tDWORD dwSequence = %lu;\n"
		"\tUINT_PTR uAppData = %p;\n"
		"}\n",
		rgdod->dwOfs, rgdod->dwData, rgdod->dwTimeStamp, rgdod->dwSequence, rgdod->uAppData
	);
}

void send_mouse_movement(const struct mouse* mouse) {
	char* to_send = realloc(NULL, 0x40);
	strcpy(to_send, "input-this mouse 0");

	char buf[0x10];
	if(mouse->x != 0) {
		snprintf(buf, sizeof(buf), " X%d", mouse->x);
		to_send = strcat(to_send, buf);
	}
	if(mouse->y != 0) {
		snprintf(buf, sizeof(buf), " Y%d", mouse->y);
		to_send = strcat(to_send, buf);
	}

	send_msg(to_send, strlen(to_send));
	free(to_send);
	if(!socket_good) {
		return;
	}

	// We can go too fast for the server at times, so block on a response
	char* block = recv_msg();
	if(!socket_good) {
		return;
	}

	free(block);
}

void recv_mouse_movement(struct mouse* mouse) {
	if(!socket_good) {
		return;
	}

	send_msg("input-what mouse 0", sizeof("input-what mouse 0"));
	if(!socket_good) {
		return;
	}

	char* msg = recv_msg();
	if(!socket_good) {
		return;
	}

	if(strlen(msg) == 0) {
		free(msg);
		return;
	}

	char* tok = msg;
	char* next;
	while(1) {
		next = strchr(tok, ' ');
		if(next != NULL) {
			*next = '\0';
		}

		char axis = tok[0];
		s32 val;
		sscanf_s(tok + 1, "%d", &val);

		if(axis == 'X') {
			mouse->x = val;
		}
		else if(axis == 'Y') {
			mouse->y = val;
		}
		else {
			LOG("[!] Invalid axis '%c' received as mouse axis\n", axis);
		}

		tok = next + 1;
		if(next == NULL) {
			break;
		}
	}

	free(msg);
}

// WIP (DirectInput mouse data)
HRESULT new_get_device_data(IDirectInputDevice8* self, DWORD object_data, DIDEVICEOBJECTDATA* rgdod, DWORD* in_out, DWORD flags) {
	return orig_get_device_data(self, object_data, rgdod, in_out, flags);

	switch(mode) {
		case TAS_NONE: {
			// static int add_click = 0;
			// static int adding_clicks = 0;

			// HRESULT resa = orig_get_device_data(self, object_data, rgdod, in_out, flags);
			// if(*in_out && rgdod && rgdod->dwOfs == 12 && rgdod->dwData == 128 && !add_click) {
			// 	add_click = 1;
			// 	rgdod->dwData = 0;
			// }
			// else if(!*in_out && add_click && !adding_clicks) {
			// 	adding_clicks = 1;
			// 	rgdod->dwOfs = 12;
			// 	rgdod->dwData = 0;
			// 	*in_out = 1;
			// }
			// else if(adding_clicks) {
			// 	add_click = 0;
			// 	adding_clicks = 0;
			// 	rgdod->dwOfs = 4;
			// 	rgdod->dwData = 0;
			// 	*in_out = 1;
			// }

			// return resa;

			// static int last_done = 1;
			// static int sign = 1;
			// if(rgdod) {
			// 	if(!last_done) {
			// 		*in_out = 0;
			// 		last_done = 1;
			// 		return DI_OK;
			// 	}
			// 	LOG("%p\n", __builtin_return_address(0));
			// 	dump_rgdod(rgdod);

			// 	rgdod->dwOfs = 0;
			// 	rgdod->dwData = sign ? 100 : -100;
			// 	//
				
			// 	sign ^= 1;
			// 	last_done = 0;
			// 	*in_out = 1;
			// }
			// else {
			// 	*in_out = 0;
			// }
			// return DI_OK;

			// LOG("loggin rgdod %p\n", __builtin_return_address(0));
			// LOG("\tsize = %lu\n\trgdod = %p\n\tio = %lu\n\tflags = %lu\n", object_data, rgdod, *in_out, flags);
			// if(rgdod) dump_rgdod(rgdod);
			// HRESULT res = orig_get_device_data(self, object_data, rgdod, in_out, flags);
			// LOG("io out = %lu\n", *in_out);
			// if(rgdod) dump_rgdod(rgdod);

			// if(rgdod && rgdod->dwOfs == 4) {
			// 	rgdod->dwData = 1;
			// }
			// return res;
			return orig_get_device_data(self, object_data, rgdod, in_out, flags);
		}
		case TAS_RECORD: {
			HRESULT res = orig_get_device_data(self, object_data, rgdod, in_out, flags);
			if(*in_out && rgdod) {
				if(rgdod->dwOfs == 0) {
					if(mouse_movement.x != 0) {
						LOG("multiple x in 1 frame! %d %d\n", mouse_movement.x, (s32)rgdod->dwData);
					}
					mouse_movement.x += (s32)rgdod->dwData;
				}
				else if(rgdod->dwOfs == 4) {
					if(mouse_movement.y != 0) {
						LOG("multiple y in 1 frame! %d %d\n", mouse_movement.y, (s32)rgdod->dwData);
					}
					mouse_movement.y += (s32)rgdod->dwData;
				}
			}
			return res;
		}
		case TAS_REPLAY: {
			DWORD orig_io = *in_out;
			HRESULT res = orig_get_device_data(self, object_data, rgdod, in_out, flags);
			LOG("[\nrgdod = %p; *in_out = %lu -> %lu\nx = %d; %d\ny = %d; %d\n", rgdod, orig_io, *in_out, mouse_movement.x, mouse_movement.x_hooked, mouse_movement.y, mouse_movement.y_hooked);

			// Override the responses if possible.
			if(*in_out && rgdod->dwOfs == 0 && mouse_movement.x != 0 && !mouse_movement.x_hooked) {
				rgdod->dwData = mouse_movement.x;
				mouse_movement.x_hooked = 1;
			}
			else if(*in_out && rgdod->dwOfs == 4 && mouse_movement.y != 0 && !mouse_movement.y_hooked) {
				rgdod->dwData = mouse_movement.y;
				mouse_movement.y_hooked = 1;
			}

			// Otherwise, tag them along at the end.
			else if(*in_out == 0 && mouse_movement.x != 0 && !mouse_movement.x_hooked) {
				rgdod->dwOfs = 0;
				rgdod->dwData = mouse_movement.x;
				mouse_movement.x_hooked = 1;

				*in_out = 1;
			}
			else if(*in_out == 0 && mouse_movement.y != 0 && !mouse_movement.y_hooked) {
				rgdod->dwOfs = 4;
				rgdod->dwData = mouse_movement.y;
				mouse_movement.y_hooked = 1;

				*in_out = 1;
			}

			LOG("rgdod = %p; *in_out = %lu\nx = %d; %d\ny = %d; %d\n]\n", rgdod, *in_out, mouse_movement.x, mouse_movement.x_hooked, mouse_movement.y, mouse_movement.y_hooked);
			
			return res;
		}
		default: {
			__builtin_unreachable();
		}
	}
}

// align len to 4
static void hexdump(const char* data, size_t len) {
	LOG("        00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
	for(size_t i = 0; i < len; i += 0x10) {
		LOG("\n%04zX   ", i);
		for(size_t j = i; j < i + 0x10 && j < len; j += 4) {
			LOG(" %02hhx %02hhx %02hhx %02hhx", data[j], data[j + 1], data[j + 2], data[j + 3]);
		}
	}
	LOG("\n\n");
}

s32 new_rand() {
	static u32 old_frame = ~0;
	static s32 value = 0;
	static u32 idx = 0;

	s32 return_val;

	if(mode != TAS_NONE) {
		if(frame_counter != old_frame) {
			idx = 0;
		}

		//LOG("[%04u] Rand() call with value %d from 0x%06X in thread %lu\n", frame_counter, value, (u32)__builtin_return_address(0) & 0xFFFFFF, GetCurrentThreadId());

		return_val = sys_rand.values[idx++];
		if(idx >= sys_rand.count) {
			idx = 0;
		}
	}
	else {
		if(frame_counter != old_frame || value > 32766) {
			value = 0;
		}

		value++;

		return_val = value & 1;
	}

	old_frame = frame_counter;
	return return_val;
}

static void (*UObject_GetName)(struct UObject* this, struct FString* out) = (void*)0x14000d520;
static void (*FFrame_GetStackTrace)(struct FFrame* this, struct FString* out) = (void*)0x140038530;
static struct FString* (*FName_GetNameString)(struct FName *this, struct FString* str) = (void*)0x1400550e0;
static void (*appFree)(void* ptr) = (void*)0x140031560;

// caller should free return value
static size_t fstring_to_utf8(const struct FString* str, char** output) {
	*output = malloc(str->length * 4);
	return wcstombs(*output, str->str, str->length * 4); 
}

static void destroy_fstring(struct FString* this) {
	this->length = 0;
	if(this->str) {
		appFree(this->str);
		this->str = NULL;
	}
}

static char* name_index_to_utf8(s32 index) {
	struct FName name = {.name_index = index, .id = 0};
	struct FString str;

	FName_GetNameString(&name, &str);
	
	char* out;
	fstring_to_utf8(&str, &out);
	
	destroy_fstring(&str);
	return out;
}

void new_urand(struct UObject* this, struct FFrame* ctx, s32* out) {
	struct FString name;
	UObject_GetName(this, &name);

	char* obj_name;
	fstring_to_utf8(&name, &obj_name);

	static u32 old_frame = ~0;
	static u32 idx = 0;

	// Compute argument value (and advance bytecode)
	s32 arg_value;
	run_expr(ctx, &arg_value);

	//orig_urand(this, ctx, out);
	if(mode != TAS_NONE) {
		if(frame_counter != old_frame) {
			idx = 0;
		}

		if(did_u) {
			*out = urand.values[idx++] % arg_value;
			if(idx >= urand.count) {
				idx = 0;
			}
		}
		else {
			*out = new_rand() % arg_value;
		}

		LOG("[%04u] Rand(%d) - %p -> %d\n", frame_counter, arg_value, ctx->pc, *out);
	}
	else {
		*out = new_rand() % arg_value;
	}

	LOG("[%04u] %s::Rand(%d) - %p -> %d\n", frame_counter, obj_name, arg_value, ctx->pc, *out);

	free(obj_name);
	destroy_fstring(&name);

	// struct FString trace;
	// FFrame_GetStackTrace(ctx, &trace);
	
	// char* trace_str;
	// fstring_to_utf8(&trace, &trace_str);
	// LOG("%s\n", trace_str);
	// free(trace_str);
	// destroy_fstring(&trace);

	old_frame = frame_counter;
}

void new_frand(struct UObject* this, struct FFrame* ctx, float* out) {
	static u32 old_frame = ~0;
	static u32 idx = 0;
	const float mult = *(float*)&(u32){0x38000000}; // 0.00003052f

	orig_frand(this, ctx, out);
	if(mode != TAS_NONE) {
		if(frame_counter != old_frame) {
			idx = 0;
		}
		
		if(did_f) {
			*out = frand.values[idx++] * mult;
			if(idx >= frand.count) {
				idx = 0;
			}
		}
		else {
			*out = new_rand() * mult;
		}

		LOG("[%04u] FRand() - %p -> %f\n", frame_counter, ctx->pc, *out);
	}

	old_frame = frame_counter;
}
