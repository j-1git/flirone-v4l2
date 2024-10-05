import unittest
import subprocess
import os

import cv2
import numpy as np


class TestDefaultStreams(unittest.TestCase):
    flir_proc = None

    @classmethod
    def module_loaded(cls, module_name):
        """Checks if module is loaded"""
        lsmod_proc = subprocess.Popen(['lsmod'], stdout=subprocess.PIPE)
        grep_proc = subprocess.Popen(['grep', "^" + module_name], stdin=lsmod_proc.stdout)
        grep_proc.communicate()  # Block until finished
        return grep_proc.returncode == 0

    @classmethod
    def program_running(cls, program_name):
        """Checks if program_name is running"""
        ps_proc = subprocess.Popen(['ps -e'], shell=True, stdin=subprocess.PIPE , stdout=subprocess.PIPE)
        grep_proc = subprocess.Popen(['grep', program_name], stdin=ps_proc.stdout, stdout=subprocess.PIPE)
        out = grep_proc.communicate()[0] # Block until finished
        if grep_proc.returncode == 0:
            pid, term, cpu, process = out.decode('utf8').split()
            return True, pid
        else:
            return False, 0

    @classmethod
    def kill_process(cls, process_name):
        running, pid = cls.program_running(process_name)
        if running:
            kill_proc = subprocess.Popen(['sudo', 'kill', '-9', pid], stdin=subprocess.PIPE, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
            kill_proc.communicate()
            if not kill_proc.returncode == 0:
                raise Exception("Failure to kill {1}".format(process_name))

    @classmethod
    def load_module_v4l2loppback(cls):
        fv4l3loopback_proc = subprocess.Popen(['sudo', 'insmod', '../../v4l2loopback/v4l2loopback.ko', 'devices=3'], stdin=subprocess.PIPE, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
        fv4l3loopback_proc.communicate()
        if not fv4l3loopback_proc.returncode == 0:
            raise Exception("Unable to load v4l2loopback kernel module.")

    @classmethod
    def unload_module_v4l2loppback(cls):
        fv4l3loopback_proc = subprocess.Popen(['sudo', 'rmmod', 'v4l2loopback'], stdin=subprocess.PIPE, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
        fv4l3loopback_proc.communicate()
        if not fv4l3loopback_proc.returncode == 0:
            raise Exception("Unable to unload v4l2loopback kernel module.")

    @classmethod
    def run_flirone(cls, switch, palette):
        # this program only return on error
        # ignore resurce warning: "subprocess nnnnn is still running" as process will be killed on TearDown
        cls.flir_proc = subprocess.Popen(['sudo', '../flirone', switch, palette], stdin=subprocess.PIPE, stderr=subprocess.PIPE, stdout=subprocess.PIPE)

    @classmethod
    def setUpClass(cls):
        if not cls.module_loaded("v4l2loopback"):
            TestDefaultStreams.load_module_v4l2loppback()

        cls.kill_process("flirone")
        TestDefaultStreams.run_flirone('--dontwaitdevice', '../palettes/Iron2.raw')

    @classmethod
    def tearDownClass(cls):
        cls.kill_process("flirone")
        cls.unload_module_v4l2loppback()

    @classmethod
    def is_red_zero(cls, frame):
        slice = frame[...,2]
        return np.all((slice == 0))

    def test_video1(self):
        cap = cv2.VideoCapture(1)
        if not cap.isOpened():
            raise Exception("Camera not ready.")

        ret, frame = cap.read()
        if not ret:
            raise Exception("Camera not ready.")

        self.assertEqual(frame.shape ,(120, 160, 3), "Raw thermal stream is not 160×120 16 bit RGB")
        self.assertTrue(TestDefaultStreams.is_red_zero(frame), "R channel of every frme pixel must be 0")

    def test_video2(self):
        cap = cv2.VideoCapture(2)
        if not cap.isOpened():
            raise Exception("Camera not ready.")

        ret, frame = cap.read()
        if not ret:
            raise Exception("Camera not ready.")

        self.assertEqual(frame.shape ,(480, 640, 3), "Raw thermal stream is not 1440×1088 16 bit RGB")

    def test_video3(self):
        cap = cv2.VideoCapture(3)
        if not cap.isOpened():
            raise Exception("Camera not ready.")

        ret, frame = cap.read()
        if not ret:
            raise Exception("Camera not ready.")

        self.assertEqual(frame.shape, (128, 160, 3), "Thermal stream with overlays is not 128×160 16 bit RGB")


if __name__ == '__main__':
    unittest.main()
