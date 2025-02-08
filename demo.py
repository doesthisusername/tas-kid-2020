#!/usr/bin/env python3
import curses
import json
import os
import socket
import threading
import time
from htas import Frame, FrameList

def send_msg(sock: socket.socket, msg):
    to_send = "{:04}{}".format(len(msg), msg)
    sock.send(to_send.encode(), 0)

def display_menu(scr, items: list, y, selected=0):
    for i in range(len(items)):
        if scr.getmaxyx()[0] <= y:
            break

        if curses.has_colors():
            scr.attrset(curses.color_pair(6 if i == selected else 7))
            scr.attron(curses.A_BOLD if i == selected else curses.A_NORMAL)

        scr.move(y, 4)
        scr.clrtoeol()
        scr.addstr(y, 4, (">" if i == selected else " ") + items[i])
        y += 1

    scr.attrset(0)

def save_movie(frame_list: list):
    to_save = []
    for frame in frame_list:
        to_save.append(frame.to_dict())

    with open("{}_{}.json".format(time.time(), len(to_save)), "w") as f:
        json.dump(to_save, f, indent=4)

def load_movie(path: str) -> list:
    if path.endswith(".json"):
        with open(path, "r") as f:
            return json.load(f)
    elif path.endswith(".htas"):
        return FrameList(path).to_dict()
    else:
        return []

def update_modes() -> list:
    out_list = ["None", "Record"]
    files = os.listdir()
    to_remove = []
    for i in range(len(files)):
        if not files[i][-5:] in [".json", ".htas", ".list"]:
            to_remove.append(i)
    
    for i in range(len(to_remove)):
        files.pop(to_remove[i] - i) # Account for len(files) decreasing

    files.sort()
    out_list.extend(files)
    return out_list

# Thread-shared variables
# Filenames are appended
mode_list = []
selected_mode = 0 # None
save_last = False
quit_early = False
def server_thread(sock: socket.socket):
    global mode_list
    global selected_mode
    global save_last
    global quit_early

    last_frame_list = []
    frame_list = []
    replay = []
    current_frame: Frame
    frame_id = 0
    running_mode = selected_mode

    while True:
        data = sock.recv(4096, 0)

        # New load
        if data == b"begin-load":
            last_frame_list = frame_list[:]
            frame_list.clear()

            selected = mode_list[selected_mode]

            if selected == "Record":
                send_msg(sock, "record")
            elif selected != "None":
                if selected.endswith(".list"):
                    if len(replay) > 0:
                        replay.pop(0)
                    if len(replay) == 0 or not mode_list[running_mode].endswith(".list"):
                        replay.clear()
                        
                        with open(selected, "r") as listf:
                            lines = listf.read().splitlines()
                            for line in lines:
                                replay.append(load_movie(line.strip()))
                else:
                    replay.clear()
                    replay.append(load_movie(selected))

                frame_id = 0
                send_msg(sock, "replay")
            else:
                send_msg(sock, "none")

            running_mode = selected_mode
        
        # New frame
        elif data.startswith(b"new-frame"):
            did_quit = False

            delta = data.split(b' ')[1]
            delta_float = float(delta.decode())
            if delta_float == 0:
                delta_float = 1 / 60

            if save_last == True:
                save_movie(last_frame_list)
                save_last = False

            ending = False

            if mode_list[running_mode] == "Record":
                current_frame = Frame(delta_float)
                current_frame.randomize("urand", count=25)
                current_frame.randomize("frand", count=25)
                frame_list.append(current_frame)
            else:
                idx = frame_id
                if quit_early or frame_id > len(replay[0]) - 1:
                    idx = len(replay[0]) - 1
                    ending = True
                    quit_early = False
                    
                    if quit_early:
                        did_quit = True

                current_frame = Frame(replay[0][idx]["delta"], replay[0][idx]["fps"], replay[0][idx]["rand"]["sysrand"], replay[0][idx]["rand"]["urand"], replay[0][idx]["rand"]["frand"], replay[0][idx]["inputs"])
                frame_id += 1

            send_msg(sock, "DELTA{} FPS{} SYSTEMRAND{} URAND{} FRAND{}{}".format(
                current_frame.delta,
                current_frame.fps if current_frame.fps == 0 else 1 / current_frame.delta,
                current_frame.list_rand("sysrand"),
                current_frame.list_rand("urand"),
                current_frame.list_rand("frand"),
                " MODEnone" if ending else "")
            )

            if did_quit and mode_list[running_mode].endswith(".list"):
                replay.clear()
        
        # Recording
        elif data.startswith(b"input-this x360"):
            tokens = data.split(b' ', 3)
            if tokens[2] == b'0':
                current_frame.inputs = tokens[3].decode() if len(tokens) == 4 else ""
            
            send_msg(sock, "ack")

        # Replaying
        elif data.startswith(b"input-what x360"):
            tokens = data.split(b' ')
            if tokens[2] == b'0' and frame_id < len(replay[0]):
                send_msg(sock, current_frame.inputs)
            else:
                send_msg(sock, "")

def main(scr):
    global mode_list
    global selected_mode
    global save_last
    global quit_early

    if curses.has_colors():
        curses.init_pair(1, curses.COLOR_BLUE, 0)
        curses.init_pair(2, curses.COLOR_CYAN, 0)
        curses.init_pair(3, curses.COLOR_GREEN, 0)
        curses.init_pair(4, curses.COLOR_MAGENTA, 0)
        curses.init_pair(5, curses.COLOR_RED, 0)
        curses.init_pair(6, curses.COLOR_YELLOW, 0)
        curses.init_pair(7, curses.COLOR_WHITE, 0)

    scr.clear()
    max_y, max_x = scr.getmaxyx()
    list_y = 2
    list_max_y = max_y - 2

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(("localhost", 8079))
    except ConnectionRefusedError:
        print("Failed to connect, press any key to exit")
        scr.refresh()
        scr.getch()
        quit()

    server = threading.Thread(target=server_thread, args=[sock])
    server.start()

    scr.nodelay(True)
    counter = 0
    while True:
        key = scr.getch()
        if key != -1:
            if key == curses.KEY_UP or key == 450:
                selected_mode = selected_mode - 1 if selected_mode > 0 else len(mode_list) - 1
                display_menu(scr, mode_list, list_y, selected_mode)

            elif key == curses.KEY_DOWN or key == 456:
                selected_mode = selected_mode + 1 if selected_mode < len(mode_list) - 1 else 0
                display_menu(scr, mode_list, list_y, selected_mode)
            
            # S key
            elif key == 115:
                save_last = True

            # Q key
            elif key == 113:
                quit_early = True
        
        # Update with new files approx every 160ms
        if (counter & 0x0F) == 0x00:
            mode_list = update_modes()
            display_menu(scr, mode_list, list_y, selected_mode)
            scr.refresh()

        counter += 1
        time.sleep(0.01)
    
if __name__ == "__main__":
    curses.wrapper(main)
