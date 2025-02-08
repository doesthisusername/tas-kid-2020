#pragma once

#include <windows.h>
#include <xinput.h>
#include "types.h"
#include "hook.h"

enum tas_mode {
    TAS_NONE,
    TAS_REPLAY,
    TAS_RECORD
};

extern enum tas_mode mode;

u32 x360_btn_to_num(char* btn);

void parse_rand(struct rand_info* info, char* list);
