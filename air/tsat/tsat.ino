#include <RFM69.h>
#include <RFM69_ATC.h>
#include <SPIFlash.h>
#include <SPI.h>

// -----[ Network Config ]-----
#define NODEID 99
#define GATEWAYID 1
#define NETWORKID 100
#define FREQUENCY RF69_433MHZ
#define ENCRYPTKEY "TSAT-2B/25"

// Auto Transmission Control
// Saves power
#define ENABLE_ATC

// -----[ Constants ]-----

#define PACKET_PING 1
#define PACKET_TELEMETRY 2

#define SATELLITE_ID 1

// -----[ Statics ]-----
#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

// -----[ Networking ]-----
// Packets are sent as raw bytes.
// While this makes things a little more complicated on the receiving end,
// It means we need basically no code for serialization/deserialization
// It also makes it very lightweight (not that it matters)

typedef struct {
  unsigned int index;
  unsigned long time; // milliseconds
  double temperature; // celsius
  double pressure; // pa
  double altitude; // m
  float accel[3]; // m/s/s
} DataPoint;

// Telemetry Packet
typedef struct {
  uint32_t index;
  uint32_t time;
  // These values are fixed-point with 3 decimals.
  // Ideally they would be floating point, but floating point
  // representation is very weird (undefined?) across platorms.
  // This might not be entirely necessary, but it's good to make sure.
  // Telemetry doesn't need to be 100% accurate anyways.
  uint32_t altitude; 
  uint32_t pressure; 
  uint32_t temperature;
  uint32_t acceleration_magnitude;
  uint32_t velocity;
} PacketTelemetry;

// Ping packet
typedef struct {
  // Counter that we sent back to identify which ping it is
  uint32_t counter;
} PacketPing;

// Metadata for each packet
typedef struct {
  uint8_t satellite_id;
  uint8_t message_type;
} PacketMeta;

typedef struct {
  PacketMeta meta;
  union {
    PacketTelemetry telemetry;
    PacketPing ping;
  } data;
} Packet;

static TaskHandle_t communcation_rx_handle;

// Receives a packet from the communications module.
// Parameters:
//   - packet: A pointer to the struct of which
//       the packet is written to.
// Returns:
//    - whether receiving the packet was successful.
bool receive_packet(Packet* packet) {
  if (!radio.receiveDone())  {
    return false;
  }
  if (radio.DATALEN < sizeof(PacketMeta)) {
    return false;
  }

  // Read in packet header
  memcpy(packet, radio.DATA, sizeof(PacketMeta));

  // Depending on which packet type we received, we
  // have to read a different amount of bytes.
  int size;
  switch (packet->meta.message_type) {
    case PACKET_PING:
      size = sizeof(PacketPing);
      break;
    case PACKET_TELEMETRY:
      size = sizeof(PacketTelemetry);
      break;
    default:
      // Unknown packet.
      return false;
  }

  // Read in the actual packet data into the data portion.
  memcpy(&packet->data, radio.DATA, size);

  return true;
}

// Converts a datapoint into a telemetry packet.
// Parameters:
//   - data: A pointer to the datapoint we are converting.
//   - (optional) previous: A pointer to the previous datapoint. 
// Returns:
//    - A telemetry packet which can be sent.
Packet datapoint_to_telemetry(DataPoint* data, DataPoint* previous) {
  // Magnitude of 3d vector (search it up)
  double accel_mag = sqrt(sq(data->accel[0]) + sq(data->accel[1]) + sq(data->accel[2]));

  // Velocity = dx / dt
  // If we don't have a previous datapoint, default to 0
  double velocity = previous ? (data->altitude - previous->altitude) / (data->time - previous->time) : 0;

  Packet packet;
  packet.meta = { SATELLITE_ID, PACKET_TELEMETRY };
  packet.data.telemetry = { 
    data->index,
    data->time,
    // Floating-point to fixed-point 3 decimals. See above for rationale.
    data->altitude * 1000,
    data->pressure * 1000,
    data->temperature * 1000,
    accel_mag * 1000,
    velocity * 1000,
  };

  return packet;
}

// Handles receiving a ping packet.
void handle_ping_packet(Packet* packet) {
  // Send back the same packet that received.
  radio.send(GATEWAYID, packet, sizeof(PacketMeta) + sizeof(PacketPing));
}

// The main loop for receiving packets.
void communication_rx(void* _) {
  while (1) {
    Packet packet;
    bool success = receive_packet(&packet);

    switch (packet.meta.message_type) {
      case PACKET_PING:
        handle_ping_packet(&packet);
        break;
      // We never should receive a telemetry packet.
      // If we do then just ignore it.
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void setup() {
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  radio.setHighPower(); // needed for RFM69HCW
  radio.encrypt(ENCRYPTKEY);
  // put your setup code here, to run once:
  xTaskCreatePinnedToCore(
    communication_rx,
    "communication_rx",
    3000,
    NULL,
    10,
    &communcation_rx_handle,
    0
  );
}

void loop() {
  // put your main code here, to run repeatedly:

}
