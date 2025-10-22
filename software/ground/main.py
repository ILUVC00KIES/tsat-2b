import struct
import enum
from dataclasses import dataclass
import typing
import serial
import argparse
import numpy as np
import matplotlib.pyplot as plt
import time
import matplotlib.animation as animation
import threading

class PacketType(enum.Enum):
    PING = 1
    TELEMETRY = 2

@dataclass
class PingPacket:
    counter: int

# Ideally this would be a 1 to 1 representation
# of the C code, but to make things simpler the
# fixed-point numbers are made floating during deserialization
@dataclass
class TelemetryPacket:
    frame_count: int
    time: float
    altitude: float
    pressure: float
    temperature: float
    acceleration_magnitude: float
    velocity: float

@dataclass
class PacketMeta:
    satellite_id: int
    packet_type: PacketType

@dataclass
class Packet:
    meta: PacketMeta
    data: TelemetryPacket | PingPacket

MAGIC = [ 84, 83, 65, 84 ]
telemetry_data: list[TelemetryPacket] = []

def deserialize_packet(raw: bytes) -> typing.Optional[Packet]:
    # see https://docs.python.org/3/library/struct.html
    # esp32 is little endian
    # <2B = little endian, 2 bytes
    id, ty = struct.unpack("<2B", raw[:2])
    try:
        ty = PacketType(ty)
    except ValueError:
        print(f"Unknown packet: {ty}")
        return None
    
    meta = PacketMeta(id, ty)
    data = None
    match meta.packet_type:
        case PacketType.PING:
            # <I = little endian, 1 32bit uint
            data = PingPacket(struct.unpack("<I", raw[2:]))
        case PacketType.TELEMETRY:
            # <7I = little endian, 7 32bit uints
            raw = list(struct.unpack("<7I"))
            # 1-6 are fixed-point floats with 3 decimals
            # Keep in mind range ends are exclusive!!
            for i in range(1, 7):
                raw[i] /= 1000

            data = TelemetryPacket(*raw)
    
    return Packet(meta, data)

def serialize_packet(packet: Packet) -> bytes:
    # <2B = little endian, 2 bytes
    b = struct.pack("<2B", packet.meta.satellite_id, packet.meta.packet_type)

    match packet.meta.packet_type:
        case PacketType.PING:
            b += struct.pack("<I", packet.data.counter)
        # We never send telemetry packets so we dont need the code for it

    return b

fig, axs = plt.subplots(2, 3)
def create_plot(fancy_name: str, label: str, data_name: str, row: int, col: int):
    ax = axs[row, col]
    line = ax.plot([], [])
    ax.grid()
    ax.set_xlim(0, 1)
    ax.set_ylim(-100, 100)
    ax.set_title(fancy_name)
    ax.set_ylabel(label)
    xdata, ydata = [], []

    def run(data):
        t, y = data
        if t is None:
            return
        xdata.append(t)
        ydata.append(y)
        xmin, xmax = ax.get_xlim()

        if t >= xmax:
            ax.set_xlim(xmin, xmax*2)
            ax.figure.canvas.draw()
        line[0].set_data(xdata, ydata)

    def data_gen():
        i = 0
        while True:
            if len(telemetry_data) > i:
                packet = telemetry_data[i]
                i += 1
                yield packet.frame_count, getattr(packet, data_name)
            else:
                yield None, None

    return animation.FuncAnimation(fig, run, data_gen, save_count=100)

temp_anim = create_plot("Temperature", "°C", "temperature", 0, 0)
pressure_anim = create_plot("Pressure", "pa", "pressure", 0, 1)
alt_anim = create_plot("Altitude", "m", "altitude", 0, 2)
accel_anim = create_plot("Acceleration", "m/s/s", "acceleration_magnitude", 1, 0)
vel_anim = create_plot("Ascent Velocity", "m/s", "velocity", 1, 1)
img_ax = axs[1, 2]
with open("ksc.png", "rb") as f:
    image = plt.imread(f)
img_ax.imshow(image)
img_ax.axis("off")

parser = argparse.ArgumentParser(
    description = "TSAT-2B Communications"
)
parser.add_argument("reciever", help="The serial port in which the reciever is connected to the computer.\nUsually COM1 on Windows and /dev/ttyUSB0 or /dev/ttyS0 on Linux/Mac")
args = parser.parse_args()
#ser = serial.Serial(args.reciever)

running = True
def run():
    i=0
    while running:
        time.sleep(1)
        telemetry_data.append(TelemetryPacket(
            i,
            i * 0.1,
            0,
            i * 10,
            40*np.sin(i),
            0,
            0
        ))
        i += 1
        # Simple code to read every packet.
        # Eventually we need to do
        # reading and writing simultaneously
        #raw = ser.readline()
        #line = bytes([int(i) for i in raw.split(' ')])
        #packet = deserialize_packet(line)
        #print("Got packet!", packet)
        #if packet.meta.packet_type == PacketType.TELEMETRY:
        #    telemetry_data.append(packet.data)

t = threading.Thread(target=run)
t.start()

plt.show(block=True)
running = False
t.join()