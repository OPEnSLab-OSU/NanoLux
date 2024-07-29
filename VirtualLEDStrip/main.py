from enum import Enum
import os
from nanolux_serial import NanoluxSerial
import customtkinter as ct
import numpy as np
import PIL
import tkinter as tk
from threading import Thread

# Enum for handing device state.
class State(Enum):
    DISCONNECTED = 0
    CONNECTING = 1
    CONNECTED = 2
    EXITING = 3

# Core class for the simulator application
class App:
    def __init__(self):
        # The "default" PIL image to pull from when generating the display bars
        self.temp = PIL.Image.fromarray(np.zeros((100, 850, 3), np.uint8))

        self.state = State.DISCONNECTED
        self.ser = NanoluxSerial()
        self.slider_value = 0.5
        
        # Scans and populates an initial array for serial port options
        self.old_serial_options = self.ser.scan()
        if "COM1" in self.old_serial_options:
            self.old_serial_options.remove("COM1")
        
        # Create the UI
        self.__create_ui_elements()
        self.__pack_ui_elements()

        # Flags
        self.connect_trigger = False
        self.stop_trigger = False
        self.mode_trigger = False
        self.is_dark_mode = True

        # State Machine List
        # Alternative to a switch/match case to decrease
        # indent count.
        self.sm = [
            self.__disconnected_state,
            self.__connecting_state,
            self.__connected_state,
        ]

        # Background thread for managing tasks like display bar
        # image generation and serial reads.
        self.background_thread = Thread(target=self.__background_process)
        self.background_thread.start()

        # Start the repeating UI update task
        self.root.after(50, self.ui_update)

        # Start the UI
        self.root.mainloop()
    
    # Function for keeping the UI updated
    #
    # Runs repeatedly as it contains it's own scheduler, which
    # is on the final line of the function.
    def ui_update(self):

        # If the mode flag is raised, switch to the other visual mode (light/dark)
        if self.is_dark_mode and self.mode_trigger:
            ct.set_appearance_mode("light")
            ct.set_default_color_theme("blue")
            self.toggle_mode_button.configure(text="Toggle Dark Mode")
        elif not self.is_dark_mode and self.mode_trigger:
            ct.set_appearance_mode("dark")
            ct.set_default_color_theme("dark-blue")
            self.toggle_mode_button.configure(text="Toggle Light Mode")     
        
        # Reset the mode flag and change the current mode boolean
        if self.mode_trigger:
            self.is_dark_mode = not self.is_dark_mode
            self.mode_trigger = False      
        
        # Update the UI display bars with their currently saved images
        self.hsv_label.configure(image=self.hsv_image)
        self.bgr_label.configure(image=self.bgr_image)
        self.rgb_label.configure(image=self.rgb_image)

        # Set the text of the connect/disconnect button based on state
        if self.state == State.DISCONNECTED:
            self.connect_button.configure(text="Connect")
        else:
            self.connect_button.configure(text="Disconnect")
        
        # Schedule another run of this function in 50 milliseconds.
        self.root.after(50, self.ui_update)
    
    # Function to raise the trigger to switch the
    # color mode of the application.
    def __raise_mode_trigger(self):
        self.mode_trigger = True
    
    # Function to raise the trigger to begin a connection.
    def __raise_connect_trigger(self):
        self.connect_trigger = True
    
    # Event handler that runs when the slider's value is changed.
    def __update_slider(self, value):
        self.slider_value = value
    
    # Event handler to start a serial port scan.
    def __start_scan(self):
        options = self.ser.scan()
        if "COM1" in options:
            options.remove("COM1")
        self.port_menu.configure(values=options)
        self.port_menu.set("None")
    
    # Creates all the UI elements on the CustomTkinter UI.
    # Does NOT place them.
    def __create_ui_elements(self):
        self.root = ct.CTk()
        self.options_frame = ct.CTkFrame(master=self.root)
        self.title_label_a = ct.CTkLabel(master=self.options_frame, text="Nanolux", height=40, font=("Roboto", 40), width=300)
        self.title_label_b = ct.CTkLabel(master=self.options_frame, text="LED Strip Simulator", font=("Roboto", 22))
        self.scan_button = ct.CTkButton(master=self.options_frame, text="Scan COM Ports", command=self.__start_scan)
        self.port_menu = ct.CTkOptionMenu(master=self.options_frame, values=self.old_serial_options)
        self.connect_button = ct.CTkButton(master=self.options_frame, text="Connect", command=self.__raise_connect_trigger)
        self.toggle_mode_button = ct.CTkButton(master=self.options_frame, text="Toggle Light Mode", command=self.__raise_mode_trigger)
        self.scalar_slider_title = ct.CTkLabel(master=self.options_frame, text="Color Scaling", font=("Roboto", 15))
        self.scalar_slider = ct.CTkSlider(master=self.options_frame, width=200, from_=0.1, to=1, command=self.__update_slider)
        self.hsv_text = ct.CTkLabel(master=self.root, text="HSV", font=("Roboto", 15))
        self.hsv_frame = ct.CTkFrame(master=self.root, width=850, height=100)
        self.bgr_text = ct.CTkLabel(master=self.root, text="BGR", font=("Roboto", 15))
        self.bgr_frame = ct.CTkFrame(master=self.root, width=850, height=100)
        self.rgb_text = ct.CTkLabel(master=self.root, text="RGB", font=("Roboto", 15))
        self.rgb_frame = ct.CTkFrame(master=self.root, width=850, height=100)
        self.hsv_image = ct.CTkImage(light_image=self.temp, dark_image=self.temp, size=(850,100))
        self.hsv_label = ct.CTkLabel(master=self.hsv_frame, width=850, height=100, image=self.hsv_image, text="")
        self.bgr_image = ct.CTkImage(light_image=self.temp, dark_image=self.temp, size=(850,100))
        self.bgr_label = ct.CTkLabel(master=self.bgr_frame, width=850, height=100, image=self.bgr_image, text="")
        self.rgb_image = ct.CTkImage(light_image=self.temp, dark_image=self.temp, size=(850,100))
        self.rgb_label = ct.CTkLabel(master=self.rgb_frame, width=850, height=100, image=self.rgb_image, text="")
    
    # Generates a generic padding object and packs it.
    def __pack_padding_object(self, x, y):
        l = ct.CTkLabel(master=self.options_frame, text="", font=("Roboto", 1))
        l.pack(padx=y, pady=x, anchor="n")
    
    # Packs all elements into their desired positions on the UI.
    def __pack_ui_elements(self):
        self.root.geometry("1200x450")
        self.root.resizable(width=False, height=False)

        if os.path.isfile("assets/icon.ico"):
            self.root.iconbitmap("assets/icon.ico")
        else:
            self.root.iconbitmap("temp/assets/icon.ico")

        
        self.root.title("Nanolux LED Strip Simulator")
        self.options_frame.pack(pady=0, padx=0, fill="y", expand=True, anchor="nw", side=tk.LEFT)
        self.__pack_padding_object(0,20)
        self.title_label_a.pack(padx=0, pady=0, anchor="n")
        self.title_label_b.pack(padx=0, pady=10, anchor="n")
        self.__pack_padding_object(0,50)
        self.scan_button.pack(padx=0, pady=0)
        self.port_menu.pack(padx=0, pady=10)
        self.connect_button.pack(padx=0, pady=0)
        self.scalar_slider_title.pack(padx=0, pady=10, anchor="n")
        self.scalar_slider.pack(padx=0, pady=0)
        self.__pack_padding_object(0,50)
        self.toggle_mode_button.pack(padx=0, pady=0)
        self.hsv_text.pack(padx=25, pady=5, anchor="w")
        self.hsv_frame.pack(pady=0, padx=25, anchor="nw")
        self.bgr_text.pack(padx=25, pady=5, anchor="w")
        self.bgr_frame.pack(pady=0, padx=25, anchor="nw")
        self.rgb_text.pack(padx=25, pady=5, anchor="w")
        self.rgb_frame.pack(pady=0, padx=25, anchor="nw")
        self.hsv_label.pack()
        self.bgr_label.pack()
        self.rgb_label.pack()
    
    # Function that runs when the application is
    # connecting to the Audiolux.
    def __connecting_state(self):
        port = self.port_menu.get()
        if self.ser.connect(port):
            self.state = State.CONNECTED
            print("Connected to " + port)

    # Function that runs when the application is
    # connected to the Audiolux.
    def __connected_state(self):
        if self.ser.read(self.slider_value):
            self.hsv_image = self.ser.get_hsv()
            self.bgr_image = self.ser.get_bgr()
            self.rgb_image = self.ser.get_rgb()
        else:
            self.state = State.DISCONNECTED
            self.__start_scan()
    
    # Function that runs when the application does not
    # know it's state.
    # Sets the backend to exit gracefully.
    def __null_state(self):
        self.state = State.EXITING
    
    # Function that runs when the application is
    # disconnected from the Audiolux.
    def __disconnected_state(self):
        self.ser.disconnect()
    
    # Function to run the state machine and all state functions.
    def __state_machine(self):
        if self.state.value > len(State):
            self.__null_state()
        else:
            self.sm[self.state.value]()
    
    # Function to run the background process responsible for
    # state machine and serial updates.
    # Is the main function of the background tasks thread.
    def __background_process(self):
        while self.state != State.EXITING:

            if self.connect_trigger and self.state == State.DISCONNECTED:
                self.state = State.CONNECTING
            elif self.connect_trigger:
                self.state = State.DISCONNECTED
            
            self.connect_trigger = False

            self.__state_machine()

if __name__ == "__main__":
    app = App()
    app.state = State.EXITING