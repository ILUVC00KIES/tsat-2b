# TSAT-2B
Software for Knights Satellite Club Tethered-Sat 2B

## Telemetry Pipeline

### Air
The satellite is launched in to the air, and reads data from sensors. This includes:
- Pressure
- Temperature
- Acceleration

From this data, we can use pressure to calculate altitude, and altitude + time to calculate ascent velocity. This data is then sent over the RFM communications module to the ground.

### Ground Pipe
This is a program ran on an ESP32 (but can be adapted to any other microcontroller) on the ground. This simply reads in packets from the RFM communications module, and relays it over serial to a laptop, where the ground program will be ran.

### Ground
This receives data over serial, and deserializes it. It then uses matplotlib to display this data as a collection of graphs.