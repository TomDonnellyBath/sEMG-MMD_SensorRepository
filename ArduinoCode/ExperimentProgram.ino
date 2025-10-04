/* 
	This program operates an Arduino host that connects by Serial port to the PC display software and by I2C to the sensors. The host is responsible for the timing of sEMG reads, passing commands regarding impedance and temperature sampling on to the sensors, and packeting all recorded data before sending on to the PC display software. 
	Written for Arduino IOT 33. (SAMD21 board)
*/

#include <Wire.h>


#define EMG_DATA_LENGTH 100 // At 2 bytes per sample, 2 sensors, and a sampling rate of 500 Hz, this buffer is 25 ms long. 
#define IMP_DATA_LENGTH 16
#define TMP_DATA_LENGTH 4
#define CMD_DATA_LENGTH 4

// Enum for commands accepted by the sensor
// Most commands act like a register access interface, a request is placed by an I2C write call and the sensors fill an output buffer with the requested data, a subsequent I2C read call is placed for the expected number of bytes. Similarly, where data is written, a subsequent I2C write call is placed. 

enum {
  SEN_EMG = 0x0, // Requests an EMG sample be made and places the previous sample into the buffer (2 bytes)
  SEN_IMP_1_REAL, // Requests the real component of the AD5933 read from electrode pair 1 (4 bytes)
  SEN_IMP_1_IMG, // Requests the imaginary component of the AD5933 read from electrode pair 1 (4 bytes)
  SEN_IMP_2_REAL, // Requests the real component of the AD5933 read from electrode pair 2 (4 bytes)
  SEN_IMP_2_IMG, // Requests the imaginary component of the AD5933 read from electrode pair 2 (4 bytes)
  SEN_IMP_ALL, // Requests the full 16 bytes of the AD5933 (Real_1, Img_1, Real_2, Img_2)
  SEN_TEMP, // Requests the MAX30205 temperature sensor data (2 bytes)
  SEN_EMG_PERIOD, // *Deprecated* Update the EMG sampling period 
  SEN_IMP_PERIOD, // *Deprecated* Update the impedance sampling period
  SEN_TEMP_PERIOD, // *Deprecated* Update the temperature sampling period 
  SEN_REQ_READ, // Request the sensor to begin an impedance and subsequent temperature sensor read
  SEN_READ_RDY, // Poll the sensor for impedance and temperature read completion (1 byte boolean)
  SEN_STOP_IMP_PER, // Stops the periodic sampling of the impedance and temperature
  SEN_START_IMP_PER, // Starts the periodic sampling of the impedance and temperature
  SEN_SET_AD_RANGE, // Set the AD5933 output range
  SEN_SET_AD_PGA, // Set the AD5933 *TODO*
  SEN_SET_REF_SW // *Deprecated* Switch the reference input electrode to the EMG or AD5933 subsystem
};

// Enum for commands recieved from the PC display software 

enum {
  UNI_OPEN = 0x0, // Command to poll if the serial port has been opened
  UNI_CHECK_SEN, // Command requesting response as to whether both sensors are connected
  UNI_IMP_TMP, // Command to issue impedance and temperature read to the sensors
  UNI_STOP_IMP_PER, // Command to issue stop for periodic impedance and temperature read
  UNI_START_IMP_PER, // Command to issues start for periodic impedance and temperature read
  UNI_SET_AD_RANGE_1, // Set AD5933 output range to option 1
  UNI_SET_AD_RANGE_2, // Set AD5933 output range to option 2
  UNI_SET_AD_RANGE_3, // Set AD5933 output range to option 3
  UNI_SET_AD_RANGE_4, // Set AD5933 output range to option 4
  UNI_SET_AD_PGA_1, // Set AD5933 gain to 1
  UNI_SET_AD_PGA_5, // Set AD5933 gain to 5
  UNI_SET_REF_SW_IMP, // Command to set reference switch to the AD5933 subsystem
  UNI_SET_REF_SW_EMG, // Command to set reference switch to the EMG subsystem
};

// Data buffers. Headers and footers used to wrap buffers with 8 know bytes that the PC software can check for to identify what data packet has been recieved.
byte emg_data[EMG_DATA_LENGTH]; // EMG data buffer
byte emg_cmd[CMD_DATA_LENGTH] = {'E', 'M', 'G', ':'}; // EMG header
byte emg_cmd_end[CMD_DATA_LENGTH] = {':', 'G', 'M', 'E'}; // EMG footer 
byte imp_data[IMP_DATA_LENGTH]; // Impedance data buffer
byte imp_cmd[CMD_DATA_LENGTH] = {'I', 'M', 'P', ':'}; // Impedance header
byte imp_cmd_end[CMD_DATA_LENGTH] = {':', 'P', 'M', 'I'}; // Impedance footer
byte tmp_data[TMP_DATA_LENGTH]; // Temperature data buffer
byte tmp_cmd[CMD_DATA_LENGTH] = {'T', 'M', 'P', ':'}; // Temperature header
byte tmp_cmd_end[CMD_DATA_LENGTH] = {':', 'P', 'M', 'T'}; // Temperature footer
byte resp_cmd[CMD_DATA_LENGTH] = {'R', 'E', 'P', ':'}; // General response header (e.g. to UNI_CHECK_SEN)
byte resp_cmd_end[CMD_DATA_LENGTH] = {':', 'P', 'E', 'R'}; // General response footer

// max length of recv ommand and data
const byte numChars = 20;
char receivedChars[numChars];
boolean newData = false;

// Macros to access individual bytes of uint16 data
#define lowByteT(x) ((uint8_t) ((x) & 0xff))
#define highByteT(x) ((uint8_t) ((x) >> 8))

// Timing variables
long loop_time; // stores the time since program start in micros

long sample_timer; // stores the time of the last EMG sample taken in micros
long sample_dif; 
long led_timer; // stores the time of the last LED change in micros (1 s)
long led_period = 1000000; // LED toggle period in micros
long sample_period = 2000; // Duration between EMG samples in micros (2 ms == 500 Hz)

long sample_counter; // Used for testing samples per second 

bool imp_poll = true; // Flag to request periodic impedance and temperature	
long imp_period = 15000000; // Periodic impedance and temperature read in micros (15 s)
long imp_timer; // stores the time of the last periodic impedance and temperature read


// additional flags
bool test_mode = false; // Used to put the arduino into a simulator mode, acting as if sensors are attached when not
bool update_flag = false; // *Deprecated*


uint8_t imp_state = 0;
uint8_t imp_avg_counter = 0;

bool recording_enabled = false;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Setup Serial port and wait for connection open by PC



  sample_timer = micros(); // Initialise sample timer
  sample_counter = 0;
  led_timer = micros(); // Initlise LED timer

  Wire.begin(); 
  Wire.setClock(400000); // Setup I2C for 400 KHz

  pinMode(LED_BUILTIN, OUTPUT); 
  imp_timer = micros(); // Initialise periodic impedance and temperature timer
}



void loop() {
  loop_time = micros(); // Update loop time to most recent time since program start

  if (recording_enabled) {
    if (loop_time - sample_timer >= sample_period-100) { // check if we are within 100 us of doing an EMG sample, latch if so such that we don't do anything else and exceed the sample timer
      while(loop_time - sample_timer < sample_period) loop_time = micros(); // Wait for correct sample time
      sample_timer = loop_time; // update the last sample time to now
      if (test_mode) { // check if simulator mode and read random or real values
        getTestSamples();
      } else {
        getRealSamples();
      }
    }

	// Whilst the impedance and temperature polling is undertaken EMG sampling period is not guaranteed.
	// However, the EMG will read 0 V as the sensors are switched off for the duration.
	// Reads are continued such that the real time display on the software continues to update for visual confirmation that no issues have occured in the experiment rig.
    if (imp_state == 1) { // Polling sensor 1 state for impedance and temperature read
      if (isIATReady(0x08)) { // Call flag polling function
        readIMP(0x08, imp_data); 
        readTMP(0x08, tmp_data);
        requestImpAndTmp(0x09); // After reading new data from sensor 1, send request for read to sensor 2. Sensors are requested sequentially to prevent injection waveforms overlapping and interferring across the body with each sensor
        imp_state = 2; // Update state
      }
    } else if (imp_state == 2) { // Polling sensor 2 state for impedance and temperature read
      if (isIATReady(0x09)) { // Call flag polling function
        readIMP(0x09, imp_data + IMP_DATA_LENGTH / 2); // Shift read data in the data buffer
        readTMP(0x09, tmp_data + TMP_DATA_LENGTH / 2);
        imp_state = 3; // Update state
      }
    } else if (imp_state == 3) { // Complete transaction state, send impedance and temperature data by Serial
      Serial.write(imp_cmd, CMD_DATA_LENGTH);
      Serial.write(imp_data, IMP_DATA_LENGTH);
      Serial.write(imp_cmd_end, CMD_DATA_LENGTH);
      Serial.write(tmp_cmd, CMD_DATA_LENGTH);
      Serial.write(tmp_data, TMP_DATA_LENGTH);
      Serial.write(tmp_cmd_end, CMD_DATA_LENGTH);
      imp_state = 0;
    }

    if (imp_poll) { // If in periodic impedance and temperature mode, check if time has elapsed to update the values and send. We do not need to poll as we know the sensors are also periodically performing a read
      if (loop_time - imp_timer > imp_period) {
        imp_timer = loop_time;
        readIMP(0x08, imp_data);
        readTMP(0x08, tmp_data);
        readIMP(0x09, imp_data + IMP_DATA_LENGTH / 2);
        readTMP(0x09, tmp_data + TMP_DATA_LENGTH / 2);
        Serial.write(imp_cmd, CMD_DATA_LENGTH);
        Serial.write(imp_data, IMP_DATA_LENGTH);
        Serial.write(imp_cmd_end, CMD_DATA_LENGTH);
        Serial.write(tmp_cmd, CMD_DATA_LENGTH);
        Serial.write(tmp_data, TMP_DATA_LENGTH);
        Serial.write(tmp_cmd_end, CMD_DATA_LENGTH);
      }
    }
  }

  if (Serial.available() > 0) { // Check for any new serial data and process, checking for expected command header and footer
    recvWithStartEndMarkers();
  }

  if (newData) { // If new Serial data contained a command
    parseData(); // Check if the command is valid and execute
  }

  if (loop_time - led_timer > led_period) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Toggle the onboard LED for peace of mind that the device is operating
    led_timer = loop_time;
  }

}

// Function to create randomised EMG samples to allow testing of serial port, parsing comms, and real time display
void getTestSamples() {
  for (int i = 0; i < 3; i++) {
    emg_data[sample_counter + i] = (byte)random(0, 255);
  }
  sample_counter += 4;
  if (sample_counter == EMG_DATA_LENGTH) {
    sample_counter = 0;
    update_flag = true;
  }
}

// Function to return dummy impedance values to test parsing comms
void getTestImp() {
  imp_data[0] = 100;
  imp_data[1] = 0; // 100
  imp_data[2] = 150;
  imp_data[3] = 0;  // 150
  imp_data[4] = 200;
  imp_data[5] = 0; // 200
  imp_data[6] = 250;
  imp_data[7] = 0; // 250
}

// Function to return dummy temperature values to test parsing comms
void getTestTemp() {
  tmp_data[0] = 44;
  tmp_data[1] = 1; // 300
}

// Functon to request EMG sample to be taken by sensors and return previous sample data
void getRealSamples() {
  unsigned int vib = readEMG(0x08); // Request from sensor 1
  while(loop_time - sample_timer < 400) loop_time = micros(); // In built delay to prevent I2C ghosting on the read
  unsigned int bee = readEMG(0x09); // Request from sensor 2
  emg_data[sample_counter]   = highByteT(vib); // Assign data to EMG buffer
  emg_data[sample_counter + 1] = lowByteT(vib);
  emg_data[sample_counter + 2] = highByteT(bee);
  emg_data[sample_counter + 3] = lowByteT(bee);

  sample_counter += 4;
  if (sample_counter == EMG_DATA_LENGTH) { // Test if buffer full, send to PC if so with EMG header and footer
    sample_counter = 0;
    Serial.write(emg_cmd, CMD_DATA_LENGTH);
    Serial.write(emg_data, EMG_DATA_LENGTH);
    Serial.write(emg_cmd_end, CMD_DATA_LENGTH);
  }
}

// Function to read EMG from specific sensor at address addr
unsigned int readEMG(int addr) {
  Wire.beginTransmission(addr);
  Wire.write(SEN_EMG); // Write EMG command
  unsigned int ret = Wire.endTransmission();
  uint8_t buf[2];
  uint8_t i = 0;
  if (ret == 0) { // read 2 bytes from the sensor, one EMG sample
    Wire.requestFrom(addr, 2);
    while (Wire.available()) {
      buf[i++] = Wire.read();
    }
  }
  uint16_t val = buf[1] << 8 | buf[0];
  return val;
}

// Function to read impedance data from sensor at address addr
void readIMP(int addr, uint8_t * buf) {
  Wire.beginTransmission(addr);
  Wire.write(SEN_IMP_ALL); // Write command to request full impedance data
  unsigned int ret = Wire.endTransmission();
  uint8_t i = 0;
  if (ret == 0) { // Read 8 bytes from the sensor, 2 bytes per 4 data units (R1, I1, R2, I2)
    Wire.requestFrom(addr, 8); 
    while (Wire.available()) {
      buf[i++] = Wire.read();
    }
  }
}

// Function to read temperature data from sensor at address addr
void readTMP(int addr, uint8_t * buf) {
  Wire.beginTransmission(addr);
  Wire.write(SEN_TEMP); // Write command to request temperature data
  unsigned int ret = Wire.endTransmission();
  uint8_t i = 0;
  if (ret == 0) { // Read 2 bytes, one temperature sample
    Wire.requestFrom(addr, 2);
    while (Wire.available()) {
      buf[i++] = Wire.read();
    }
  }
}

// Function to parse data over the Serial comm from the PC software
void parseData() {
  newData = false;
  if (receivedChars[0] == UNI_OPEN) { // Command checking if the port is open
    Serial.write(resp_cmd, CMD_DATA_LENGTH);
    Serial.write("HI"); // Respond with expected string "HI" and generic response header and footer
    Serial.write(resp_cmd_end, CMD_DATA_LENGTH);
  }

  if (receivedChars[0] == UNI_CHECK_SEN) { // Command checking if the sensors are attached
    if (test_mode) {
      Serial.write(resp_cmd, CMD_DATA_LENGTH);
      Serial.write("Y"); // If simulator mode, just say yes in generic response header and footer
      Serial.write(resp_cmd_end, CMD_DATA_LENGTH);
      recording_enabled = true;
    } else {
      bool sen_one = checkSensor(0x08); // Query sensor 1
      bool sen_two = checkSensor(0x09); // Query sensor 2
      Serial.write(resp_cmd, CMD_DATA_LENGTH); // Wrap with header
      if (sen_one && sen_two) {
        Serial.write("Y"); // Say yes if both present
        recording_enabled  = true;
      } else if (sen_one) {
        Serial.write("1"); // Alert which sensor is present if one missing
      } else if (sen_two) {
        Serial.write("2");
      } else {
        Serial.write("N"); // Alert if neither sensor detected 
      }
      Serial.write(resp_cmd_end, CMD_DATA_LENGTH); // Wrap with footer
    }
  }

  // commands only available once setup and confirmed connected
  if (recording_enabled) {
    // request imp and temp reading
    if (receivedChars[0] == UNI_IMP_TMP) { //Command requesting an impedance and temperature read
      if (test_mode) { // In simulator mode, return immediately the dummy values
        getTestImp();
        getTestTemp();
        Serial.write(imp_cmd, CMD_DATA_LENGTH); // Send each sensor separately with appropriate header and footer
        Serial.write(imp_data, IMP_DATA_LENGTH);
        Serial.write(imp_cmd_end, CMD_DATA_LENGTH);
        Serial.write(tmp_cmd, CMD_DATA_LENGTH);
        Serial.write(tmp_data, TMP_DATA_LENGTH);
        Serial.write(tmp_cmd_end, CMD_DATA_LENGTH);
      } else {
        requestImpAndTmp(0x08); // If not in simulator mode, request a reading from sensor 1
        imp_state = 1; // used to not block, this way we get constant emg updates in the software
		// imp_state 1 begins the polling process in the main loop function
      }
    }

    if (receivedChars[0] == UNI_STOP_IMP_PER) { // Command to stop periodic impedance and temperature reads
      Wire.beginTransmission(0x08); // Pass on the command to sensor 1
      Wire.write(SEN_STOP_IMP_PER);
      Wire.endTransmission();
      Wire.beginTransmission(0x08); // Repeat the command, as due to the register access setup on the sensor it will always expect two commands (should have been adapted on sensors)
      Wire.write(SEN_STOP_IMP_PER);
      Wire.endTransmission();
	  
      Wire.beginTransmission(0x09); // Pass on the command to sensor 2
      Wire.write(SEN_STOP_IMP_PER);
      Wire.endTransmission();
      Wire.beginTransmission(0x09); // Repeat the command, as due to the register access setup on the sensor it will always expect two commands (should have been adapted on sensors)
      Wire.write(SEN_STOP_IMP_PER);
      Wire.endTransmission();
	  
      imp_poll = false;
    }

    if (receivedChars[0] == UNI_START_IMP_PER) { // Command to start periodic impedance and temperature reads
      Wire.beginTransmission(0x08); // Pass on the command to sensor 1
      Wire.write(SEN_START_IMP_PER);
      Wire.endTransmission();
      Wire.beginTransmission(0x08); // Repeat the command, as due to the register access setup on the sensor it will always expect two commands (should have been adapted on sensors)
      Wire.write(SEN_START_IMP_PER);
      Wire.endTransmission();
	  
      Wire.beginTransmission(0x09); // Pass on the command to sensor 2
      Wire.write(SEN_START_IMP_PER);
      Wire.endTransmission();
      Wire.beginTransmission(0x09); // Repeat the command, as due to the register access setup on the sensor it will always expect two commands (should have been adapted on sensors)
      Wire.write(SEN_START_IMP_PER);
      Wire.endTransmission();
	  
      imp_poll = true;
    }

    
	// AD5933 setup commands for adjusting gain and output voltage - unsused in actual program
    if (receivedChars[0] == UNI_SET_AD_RANGE_1) {
      setADRange(1);
    }
    if (receivedChars[0] == UNI_SET_AD_RANGE_2) {
      setADRange(2);
    }
    if (receivedChars[0] == UNI_SET_AD_RANGE_3) {
      setADRange(3);
    }
    if (receivedChars[0] == UNI_SET_AD_RANGE_4) {
      setADRange(4);
    }
    if (receivedChars[0] == UNI_SET_AD_PGA_1) {
      setADPGA(1);
    }
    if (receivedChars[0] == UNI_SET_AD_PGA_5) {
      setADPGA(5);
    }
    if (receivedChars[0] == UNI_SET_REF_SW_EMG) {
      setREFsw(0);
    }
    if (receivedChars[0] == UNI_SET_REF_SW_IMP) {
      setREFsw(1);
    }
  }
}

uint8_t writing_buf[2] = {0, 0};

// Writes the specific command value to each sensor for the reference switch
void setREFsw(uint8_t val) {
  writing_buf[1] = 0;
  writing_buf[0] = val;
  
  Wire.beginTransmission(0x08);
  Wire.write(SEN_SET_REF_SW);
  Wire.endTransmission();
  Wire.beginTransmission(0x08);
  Wire.write(writing_buf, 2);
  Wire.endTransmission();
  
  Wire.beginTransmission(0x09);
  Wire.write(SEN_SET_REF_SW);
  Wire.endTransmission();
  Wire.beginTransmission(0x09);
  Wire.write(writing_buf, 2);
  Wire.endTransmission();
}

// Writes the specific command value to each sensor for the AD5933 output voltage
void setADRange(uint8_t val) {
  writing_buf[1] = 0;
  writing_buf[0] = val;
  
  Wire.beginTransmission(0x08);
  Wire.write(SEN_SET_AD_RANGE);
  Wire.endTransmission();
  Wire.beginTransmission(0x08);
  Wire.write(writing_buf, 2);
  Wire.endTransmission();
  
  Wire.beginTransmission(0x09);
  Wire.write(SEN_SET_AD_RANGE);
  Wire.endTransmission();
  Wire.beginTransmission(0x09);
  Wire.write(writing_buf, 2);
  Wire.endTransmission();
}

// Writes the specific command value to each sensor for the AD5933 gain
void setADPGA(uint8_t val) {
  writing_buf[1] = 0;
  writing_buf[0] = val;
  
  Wire.beginTransmission(0x08);
  Wire.write(SEN_SET_AD_PGA);
  Wire.endTransmission();
  Wire.beginTransmission(0x08);
  Wire.write(writing_buf, 2);
  Wire.endTransmission();
  
  Wire.beginTransmission(0x09);
  Wire.write(SEN_SET_AD_PGA);
  Wire.endTransmission();
  Wire.beginTransmission(0x09);
  Wire.write(writing_buf, 2);
  Wire.endTransmission();

}

// Simple function to query the sensor, err 0 indicates that the sensor has ACKed on the I2C bus and is present
bool checkSensor(int addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  if (err == 0) {
    return true;
  } else {
    return false;
  }
}

// Function to request the start of an impedance and temperature read by sensor at address addr
void requestImpAndTmp(int addr) {
  Wire.beginTransmission(addr);
  Wire.write(SEN_REQ_READ);
  Wire.endTransmission(); 
  Wire.beginTransmission(addr); // Repeat the command, as due to the register access setup on the sensor it will always expect two commands (should have been adapted on sensors)
  Wire.write(SEN_REQ_READ);
  Wire.endTransmission();
}

// Function to check whether the impedance and temperature complete flag is set on sensor at address addr
uint8_t isIATReady(int addr) {
  uint8_t ret = 0;
  Wire.beginTransmission(addr);
  Wire.write(SEN_READ_RDY);
  Wire.endTransmission();
  Wire.requestFrom(addr, 1);
  while (Wire.available()) {
    ret = Wire.read();
  }
  return ret;
}

// Function used to identify commands on the serial bus. "<" and ">" markers wrap the command such that we can parse for the command within the read bytes
void recvWithStartEndMarkers()
{

  // function recvWithStartEndMarkers by Robin2 of the Arduino forums
  // See  http://forum.arduino.cc/index.php?topic=288234.0 - Last Accessed 26/08/25

  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  if (Serial.available() > 0)
  {
    rc = Serial.read();
    if (recvInProgress == true)
    {
      if (rc != endMarker)
      {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else
      {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

