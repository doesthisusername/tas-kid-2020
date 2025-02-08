#pragma once

#include <xinput.h>
#include <dinput.h>
#include "types.h"

struct FName {
    s32 name_index;
    s32 id;
};

struct FString {
    wchar_t* str;
    s32 length; // guesswork
    s32 capacity;
};

#pragma pack(push)
#pragma pack(1)
struct FFrame {
    char unk_00[0x1C];
    void* unk_ptr;
    u8* pc;
};

struct UObject {
    char unk_00[0x38];
    int initialized;
    char unk_3C[0x0C];
    struct FName name;
};
#pragma pack(pop)

struct rand_info {
    s32* values;
    u32 count;
    u32 max;
};

#define KEY_QUEUE_N 0x100
struct key_queue {
    u16* vals;
    size_t val_max;
    size_t val_n;
    size_t val_i;
};

struct mouse {
    s32 x;
    s32 y;
    s32 x_hooked;
    s32 y_hooked;
};

// Returns a function pointer
void** find_native_func(const char* name);

// Makes writable, then writes pointer, then restores
void update_func_ptr(void** old, void* new);

void replace_all_u32(u32 old, u32 new);

// Runs UnrealScript expression
void run_expr(struct FFrame* ctx, void* out);

void send_new_frame(float delta);

void reset_key_queue(struct key_queue* queue);
void add_key_queue_key(struct key_queue* queue, s32 key, u16 val);
void recv_key_states(struct key_queue* queue);
void send_key_states(struct key_queue* queue);

void send_mouse_movement(const struct mouse* mouse);
void recv_mouse_movement(struct mouse* mouse);

extern void(*orig_native_delta)(void* unk_a, struct FFrame* ctx, float* out);
void new_native_delta(void* unk_a, struct FFrame* ctx, float* out);

extern void(*orig_create_load)(void* unk_a, struct FFrame* ctx);
void new_create_load(void* unk_a, struct FFrame* ctx);

extern u32(*orig_input_key)(void* this, int param_1, struct FName key, u8 event, float param_4, u32 param_5);
u32 new_input_key(void* this, int param_1, struct FName key, u8 event, float param_4, u32 param_5);

extern u32(*orig_is_dlc_installed)(void* this, int a);
u32 new_is_dlc_installed(void* this, int app_id);

extern void(*orig_tick)(void* delta, float unk_b, float unk_a);
void new_tick(void* delta, float unk_b, float unk_a);

extern HRESULT(*orig_poll)(void* unk_a);
HRESULT new_poll(void* unk_a);

extern u32(*orig_get_state)(u32 player_id, XINPUT_STATE* state);
u32 new_get_state(u32 player_id, XINPUT_STATE* state);

extern u16(*orig_get_key_state)(s32 key_id);
u16 new_get_key_state(s32 key_id);

extern HRESULT(*orig_get_device_data)(IDirectInputDevice8* self, DWORD object_data, DIDEVICEOBJECTDATA* rgdod, DWORD* in_out, DWORD flags);
HRESULT new_get_device_data(IDirectInputDevice8* self, DWORD object_data, DIDEVICEOBJECTDATA* rgdod, DWORD* in_out, DWORD flags);

extern s32(*orig_rand)();
s32 new_rand();

extern void(*orig_urand)(struct UObject* this, struct FFrame* ctx, s32* out);
void new_urand(struct UObject* this, struct FFrame* ctx, s32* out);

extern void(*orig_frand)(struct UObject* this, struct FFrame* ctx, float* out);
void new_frand(struct UObject* this, struct FFrame* ctx, float* out);

