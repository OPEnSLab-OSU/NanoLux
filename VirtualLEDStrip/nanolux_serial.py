import serial
import cv2
import numpy as np
import math
import PIL
import customtkinter as ct
import serial.tools.list_ports

# Static function to scale an 8-bit integer exponentially.
def scale_int(value, scaling):
    return int(float(float(value / 255) ** scaling) * 255)

# Static function to convert a BGR image to an HSV and an RGB one.
def bgr_to_hsv_rgb(bgr):
    hsv = cv2.cvtColor(bgr, cv2.COLOR_BGR2HSV)
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
    return hsv, rgb

# Class for managing Nanolux communication
class NanoluxSerial:

    def __init__(self):
        self.connected = False
        self.port = "None"
        self.bgr = np.zeros((850, 100, 3), np.uint8)
        self.rgb = np.zeros((850, 100, 3), np.uint8)
        self.hsv = np.zeros((850, 100, 3), np.uint8)
        self.serial = None
        self.fails = 0
    
    # Performs a serial port scan and returns the names
    # of all COM ports attached to the machine.
    def scan(self):
        ports = serial.tools.list_ports.comports()
        str_ports = ["None"]
        for port, desc, hwid in sorted(ports):
            str_ports.append(port)
        return str_ports
    
    # Attempts a connection to an Audiolux device through
    # a given COM port.
    def connect(self, port):
        try:
            self.serial = serial.Serial(port, 115200)
            self.connected = True
            self.port = port
            return True
        except:
            self.serial = None
            return False
    
    # Disconnects from the Audiolux device's COM port.
    def disconnect(self):
        self.connected = False
        self.serial = None
        self.port = "None"
        self.bgr = np.zeros((100, 850, 3), np.uint8)

    # Read from the COM port and generate images for the display bars.
    def read(self, scaling):
        
        # Pull the serial data and decode it into a list.
        self.serial_data = self.serial.readline().decode("utf-8").split()

        # If the length of the serial list is 0, then a failure has occured.
        if len(self.serial_data) == 0:
            # Increment the fail count and return False if
            # more than 5.
            self.fails += 1
            if(self.fails > 5):
                return True
            return False

        # Calculate the LED width and reset the fail counter
        w = math.floor(850/len(self.serial_data)) 
        self.fails = 0       

        # For each pixel, color it in on the BGR image.
        for i, led_value in enumerate(self.serial_data):
            self.values = led_value.split(",")
            try:
                # Apply the recieved pixel color to all corresponding image
                # pixels.
                for j in range(i*w, (i+1)*w):
                    self.bgr[:, j] = (
                        scale_int(int(self.values[2]), scaling),
                        scale_int(int(self.values[1]), scaling),
                        scale_int(int(self.values[0]), scaling)
                    )  # (B, G, R)
            except:
                pass
        
        # Resize the image to be the size of the bar.
        self.bgr = cv2.resize(self.bgr, (850, 100))
        # Generate the HSV and RGB images.
        self.hsv, self.rgb = bgr_to_hsv_rgb(self.bgr)
        # Return a success.
        return True

    # Returns the stored HSV image.
    def get_hsv(self):
        temp = PIL.Image.fromarray(self.hsv)
        return ct.CTkImage(light_image=temp, dark_image=temp, size=(850,100))

    # Returns the stored BGR image.
    def get_bgr(self):
        temp = PIL.Image.fromarray(self.bgr)
        return ct.CTkImage(light_image=temp, dark_image=temp, size=(850,100))
    
    # Returns the stored RGB image.
    def get_rgb(self):
        temp = PIL.Image.fromarray(self.rgb)
        return ct.CTkImage(light_image=temp, dark_image=temp, size=(850,100))