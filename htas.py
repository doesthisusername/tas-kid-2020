#!/usr/bin/env python3
import random

class Frame:
    def __init__(self, delta=0.016667, fps=None, sysrand=[0], urand=[0], frand=[0], inputs=""):
        self.delta = delta
        if fps == None:
            self.fps = 1 / self.delta
        else:
            self.fps = fps

        self.rand = dict(sysrand=sysrand[:], urand=urand[:], frand=frand[:])
        self.inputs = inputs

    def list_rand(self, which):
        result = ""
        for num in self.rand[which]:
            result += "{},".format(num)
        return result[0 : -1]

    def randomize(self, which, count=10):
        self.rand[which].clear()
        for i in range(count):
            self.rand[which].append(random.randint(0, 32767))

    def to_dict(self):
        return dict(delta=self.delta, fps=self.fps, rand=self.rand, inputs=self.inputs)

class FrameList:
    def __init__(self, filename: str):
        self.from_file(filename)

    def _adjust_input(self, name: str, positive: bool):
        idx = self.cur_frame.inputs.find(name)

        if positive and idx == -1:
            self.cur_frame.inputs += " " + name
        elif idx != -1:
            self.cur_frame.inputs = self.cur_frame.inputs.replace(" " + name, "")

    def _adjust_axis(self, name: str, axis: int, value: int):
        idx = self.cur_frame.inputs.find(name)
        mid = self.cur_frame.inputs.find(";", idx)
        end = self.cur_frame.inputs.find(" ", mid)
        if end == -1:
            end = len(self.cur_frame.inputs)

        old_x = 0
        old_y = 0

        if idx != -1:
            old_x = int(self.cur_frame.inputs[idx + 7 : mid])
            old_y = int(self.cur_frame.inputs[mid + 1 : end])

        value_str = "{};{}".format(value if axis == 0 else old_x, value if axis == 1 else old_y)

        if idx != -1:
            self.cur_frame.inputs = self.cur_frame.inputs.replace(self.cur_frame.inputs[idx : end], name + value_str)
        else:
            self.cur_frame.inputs += " " + name + value_str

    def _adjust_rand(self, name: str, value: str):
        self.cur_frame.rand[name].clear()
        for num in value.split(","):
            self.cur_frame.rand[name].append(int(num))
    
    def parse_header(self, f):
        line: str = f.readline()
        while not line.isspace():
            if line.startswith("fps: "):
                self.orig_fps = 1 / float(line[5:])
                self.cur_frame.delta = self.orig_fps
            elif line.startswith("length: "):
                self.length = int(line[8:])
            elif line.startswith("type: IL"):
                self.start_buf = 122

            line = f.readline()
        
        for i in range(self.start_buf):
            self.frames.append(Frame(self.cur_frame.delta, self.cur_frame.fps, self.cur_frame.rand["sysrand"], self.cur_frame.rand["urand"], self.cur_frame.rand["frand"], self.cur_frame.inputs))

    def parse_line(self, line: str):
        if line.strip().startswith("//") or line.isspace():
            return

        up_to = int(line.split(":")[0]) + self.start_buf - 1
        while len(self.frames) < up_to:
            self.frames.append(Frame(self.cur_frame.delta, self.cur_frame.fps, self.cur_frame.rand["sysrand"], self.cur_frame.rand["urand"], self.cur_frame.rand["frand"], self.cur_frame.inputs))
        
        tokens = line.split(" ")[1:]
        for token in tokens:
            if token.startswith("//"):
                break

            token = token.rstrip()
            if token == "":
                continue

            positive = True
            if token[0] == '~':
                positive = False
                token = token[1:]

            if token in ["START", "BACK", "A", "B", "X", "Y"]:
                self._adjust_input("BTN_" + token, positive)
            elif token in ["UP", "RIGHT", "DOWN", "LEFT"]:
                self._adjust_input("BTN_DPAD_" + token, positive)
            elif token == "LB":
                self._adjust_input("BTN_LEFT_SHOULDER", positive)
            elif token == "RB":
                self._adjust_input("BTN_RIGHT_SHOULDER", positive)
            elif token == "LT":
                self._adjust_input("TRIGGER_LT", positive)
            elif token == "RT":
                self._adjust_input("TRIGGER_RT", positive)
            elif token == "L3":
                self._adjust_input("BTN_LEFT_THUMB", positive)
            elif token == "R3":
                self._adjust_input("BTN_RIGHT_THUMB", positive)
            elif token.startswith("LX"):
                self._adjust_axis("AXIS_LS", 0, int(token[3:]) if positive else 0)
            elif token.startswith("LY"):
                self._adjust_axis("AXIS_LS", 1, int(token[3:]) if positive else 0)
            elif token.startswith("RX"):
                self._adjust_axis("AXIS_RS", 0, int(token[3:]) if positive else 0)
            elif token.startswith("RY"):
                self._adjust_axis("AXIS_RS", 1, int(token[3:]) if positive else 0)
            elif token.startswith("URAND"):
                self._adjust_rand("urand", token[6:] if positive else "0")
            elif token.startswith("FRAND"):
                self._adjust_rand("frand", token[6:] if positive else "0")
            elif token.startswith("SRAND"):
                self._adjust_rand("sysrand", token[6:] if positive else "0")
            elif token.startswith("DELTA"):
                self.cur_frame.delta = float(token[6:]) if positive else self.orig_fps
            elif token == "***":
                for i in range(len(self.frames)):
                    self.frames[i].fps = 0.0

    def from_file(self, filename: str):
        self.cur_frame: Frame = Frame()
        self.frames = []
        self.start_buf = 0
        self.length = 0

        with open(filename, "r") as f:
            self.parse_header(f)
            line = f.readline()
            while line != "":
                self.parse_line(line)
                line = f.readline()
            
            while len(self.frames) < self.length + self.start_buf:
                self.frames.append(Frame(self.cur_frame.delta, self.cur_frame.fps, self.cur_frame.rand["sysrand"], self.cur_frame.rand["urand"], self.cur_frame.rand["frand"], self.cur_frame.inputs))

    def to_dict(self):
        frame_list = []
        for frame in self.frames:
            frame_list.append(frame.to_dict())
        
        return frame_list
