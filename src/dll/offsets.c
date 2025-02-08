#include "types.h"
#include "main.h"
#include "offsets.h"

struct version_info ver;

void* resolve_path(struct ptr_path* path) {
	// This is weird, basically just dereferencing though
	u8* result = *(u8**)path->base;

	if(result == NULL) {
		return NULL;
	}

	for(u32 i = 0; i < path->offset_n - 1; i++) {
		result = *(u8**)(result + path->offsets[i]);

		if(result == NULL) {
			return NULL;
		}
	}

	result += path->offsets[path->offset_n - 1];
	return result;
}

u32 get_exe_timestamp(void* base) {
	u32* ptr = base;
	ptr = (u32*)((u8*)base + ptr[0x0F]);
	return ptr[0x02];
}

u32 init_ver(u32 timestamp) {
	switch(timestamp) {
		case TS_DLC231: {
			ver.byte_op_table = (void**)((u8*)hat_base + 0x10CCB60);
			ver.xinput_get_state = (void**)((u8*)hat_base + 0xB8BC60);
			ver.msvcr_rand = (void**)((u8*)hat_base + 0xB8B6D0);
			ver.user_get_key_state = (void**)((u8*)hat_base + 0xB8B940);
			ver.input_key = (void**)((u8*)hat_base + 0xD44A78);
			ver.is_dlc_installed = (void**)((u8*)hat_base + 0xE7A140);
			ver.dinput8_mouse = (IDirectInputDevice8**)((u8*)hat_base + 0x1219548);
			ver.particle_rand = (u32*)((u8*)hat_base + 0x10B9728);

			ver.fps_path.base = (u8*)hat_base + 0x11F6F10;
			ver.fps_path.offset_n = 1;
			ver.fps_path.offsets[0] = 0x710;

			ver.uengine_tick.base = (u8*)hat_base + 0x11F6F10;
			ver.uengine_tick.offset_n = 2;
			ver.uengine_tick.offsets[0] = 0x00;
			ver.uengine_tick.offsets[1] = 0x258;

			ver.loaded_this_tick.base = (u8*)hat_base + 0x11F6F10;
			ver.loaded_this_tick.offset_n = 1;
			ver.loaded_this_tick.offsets[0] = 0x99C;
			
			return 1;
		}
		case TS_DLC232: {
			ver.byte_op_table = (void**)((u8*)hat_base + 0x10CFC30);
			ver.xinput_get_state = (void**)((u8*)hat_base + 0xB8DC60);
			ver.msvcr_rand = (void**)((u8*)hat_base + 0xB8B6D0);
			ver.user_get_key_state = NULL;
			ver.particle_rand = NULL;

			ver.fps_path.base = (u8*)hat_base + 0x11F9FE0;
			ver.fps_path.offset_n = 1;
			ver.fps_path.offsets[0] = 0x710;

			ver.uengine_tick.base = (u8*)hat_base + 0x11F9FE0;
			ver.uengine_tick.offset_n = 2;
			ver.uengine_tick.offsets[0] = 0x00;
			ver.uengine_tick.offsets[1] = 0x258;

			ver.loaded_this_tick.base = (u8*)hat_base + 0x11F9FE0;
			ver.loaded_this_tick.offset_n = 1;
			ver.loaded_this_tick.offsets[0] = 0x99C;

			return 1;
		}
		default: {
			LOG("Invalid version %u, this is really bad!\n", timestamp);
			return 0;
		}
	}
}
