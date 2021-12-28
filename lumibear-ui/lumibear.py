import kivy
from kivy.uix.accordion import Accordion
from kivy.app import App
from kivy.properties import ListProperty, StringProperty
kivy.require('1.0.7')

import socket

class Lumibear(Accordion):
    colpicker = ListProperty((1,1,1,1))
    hex_color = StringProperty("#FFFFFFFF")
    rot1_color = StringProperty("#FFFFFFFF")
    rot2_color = StringProperty("#FFFFFFFF")
    ipaddress = StringProperty("192.168.178.83")
    ipport = StringProperty(8888)

    def __init__(self, **kwargs):
        super(Lumibear, self).__init__(**kwargs)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def setColor(self, color=""):
        if "" == color:
            color = self.hex_color[1:7]
        byte_message = bytes("color?" + color, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print("Color: ", color)


    def setTwo(self):
        msg = "two?" + self.rot1_color[1:7] + "," + self.rot2_color[1:7]
        byte_message = bytes(msg, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print(msg)

    def setRotate(self):
        msg = "rotate?" + self.rot1_color[1:7] + "," + self.rot2_color[1:7]
        byte_message = bytes(msg, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print(msg)

    def setSweep(self):
        msg = "sweep?" + self.rot1_color[1:7] + "," + self.rot2_color[1:7]
        byte_message = bytes(msg, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print(msg)

    def setWave(self):
        msg = "wave?" + self.rot1_color[1:7] + "," + self.rot2_color[1:7]
        byte_message = bytes(msg, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print(msg)

    def setLighthouse(self, kennung):
        msg = "lighthouse?" + kennung
        byte_message = bytes(msg, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print(msg)

    def setBrightness(self, value):
        msg = "brightness?" + str(value)
        byte_message = bytes(msg, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print(msg)

    def setRotWait(self, value):
        msg = "rotwait?" + str(value)
        byte_message = bytes(msg, "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print(msg)

    def on(self):
        byte_message = bytes("on", "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print("on")

    def off(self):
        byte_message = bytes("off", "utf-8")
        self.socket.sendto(byte_message, (self.ipaddress, self.ipport))
        print("off")

        

class LumibearApp(App):
    def __init__(self, **kwargs):
        super(LumibearApp, self).__init__(**kwargs)
        self.title = 'Lumibear'

    def build(self):
        return Lumibear()

if __name__ == '__main__':
    LumibearApp().run()