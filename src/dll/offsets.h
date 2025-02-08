#pragma once

#include <dinput.h>
#include "types.h"

#define TS_DLC231 1561041656
#define TS_DLC232 1565114742

#define OP_RAND 0xA7
#define OP_FRAND 0xC3

struct ptr_path {
	u8* base;
	u32 offset_n;
	u64 offsets[0xC]; // Pretty bad
};

struct version_info {
	void** byte_op_table;
	void** xinput_get_state;
	void** msvcr_rand;
	void** user_get_key_state;
	void** input_key;
	void** is_dlc_installed;
	IDirectInputDevice8** dinput8_mouse;
	u32* particle_rand;
	struct ptr_path fps_path;
	struct ptr_path uengine_tick;
	struct ptr_path loaded_this_tick;
};

extern struct version_info ver;

void* resolve_path(struct ptr_path* path);
u32 get_exe_timestamp(void* base);
u32 init_ver(u32 timestamp);
