#include <string.h>
#include <windows.h>
#include <xinput.h>
#include "types.h"
#include "tas.h"

enum tas_mode mode;

u32 x360_btn_to_num(char* btn) {
#define CHECKBTN(x) if(memcmp(btn, #x, strlen(btn)) == 0) return XINPUT_GAMEPAD_ ## x
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

	return 0;
}

void parse_rand(struct rand_info* info, char* list) {
	info->count = 0;

	char* tok = list;
	char* next;
	while(1) {
		// Separate at commas
		next = strchr(tok, ',');
		if(next != NULL) {
			*next = '\0';
		}

		if(info->count >= info->max) {
			info->max += 8;
			info->values = realloc(info->values, info->max * sizeof(u32));
		}

		info->values[info->count++] = atoi(tok);

		tok = next + 1;
		if(next == NULL) {
			break;
		}
	}
}
