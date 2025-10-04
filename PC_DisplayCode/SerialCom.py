# Widget to host COM port to Arduino Host
# Has no display elements, runs a QObject based threaded Serial Port and handles singals for generating data when requested for tasks

import logging
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from PyQt5.QtSerialPort import *

import time

from Commands import cmds, cmd_wait_response

class SerialComWidget(QWidget):

    sig_emgDataReady = pyqtSignal(list) # signal emitted on reciept of new EMG packet
    sig_impTempReady = pyqtSignal(list, list) # signal emitted on reciept of new IT packet
    sig_portNotification = pyqtSignal(str)      # signal for errors/warnings/info on the com port
    sig_deviceNotification = pyqtSignal(str)    # signal for errors/warnings/info on the Arduino or Sensors
    sig_sendCommand = pyqtSignal(bytearray) # signal for sending commands to the Serial thread
    sig_cmdResponse = pyqtSignal(str) # signal emitted when the Arduino responds to a command from elsewhere in the software
    sig_serialError = pyqtSignal() # signal emitted if there is an error on the serial port
    
    max_command = len(cmds) 
    
    command_chars = 4 
    
    
    
    def __init__(self, packet_size, *args, **kwargs):
    
        super(SerialComWidget, self).__init__(*args, **kwargs)
        
        self.packet_size = packet_size
        
        self.open = False
        
        self.logger = logging.getLogger("app_logger.SerialComWidget")
        
        # has none of the standard QT things seen here as this is a widgetless widget
        self.logger.info("Setting up widgets.")
        
        
        self.logger.info("Setting up signals.")
        
        
        self.logger.info("Setting up layout.")
        
        # setup timer to poll for ports to open
        self.com_timer = QTimer()
        self.com_timer.timeout.connect(self.testSerialPorts)
        self.com_timer.setInterval(5000) # poll every 5 seconds
        self.com_timer.start()
        
        self.emg_data = [] # storage variable for incoming EMG
        
        self.logger.info("Finalising.")
        
    # initialise the software with the port closed    
    def postInit(self):
        self.sig_portNotification.emit("Closed")
        
    def resetSoftware(self):
        pass
        
    # callback function on polling timer timeout
    def testSerialPorts(self):
        for x in QSerialPortInfo().availablePorts(): # scan available com ports
            #print(x.productIdentifier()) # use these lines to identiy product and vendor IDs for any used arduinos
            #print(x.vendorIdentifier())
            if (x.productIdentifier() == 94 or x.productIdentifier() == 32858 or x.productIdentifier() == 32855) and x.vendorIdentifier() == 9025: # if port matches a known Arduino
                self.com_timer.stop() # stop the polling timer
                self.serial_thread = QThread() # instantiate a QThread 
                self.serial_obj = SerialObject(x, 115200, (self.command_chars*2)+(self.packet_size*2)) # create our serial object that contains the com port, passing the com object through
                # connect necessary signals from both the thread, the object, and the widget to permit information passing between the threads
                self.serial_thread.finished.connect(self.threadFinished) 
                self.serial_obj.sig_emgDataReady.connect(self.emgDataReady)
                self.serial_obj.sig_impAndTempDataReady.connect(self.impTmpDataReady)
                self.serial_obj.sig_cmdResponse.connect(self.procCMDResponse)
                self.serial_obj.sig_serialError.connect(self.procSerialError)
                self.sig_sendCommand.connect(self.serial_obj.sendCommand)
                self.serial_obj.moveToThread(self.serial_thread) # put the serial object onto the thread so it runs in the threads exec loop not the UI exec loop
                self.logger.info("Starting serial thread to Arduino")
                self.serial_thread.start() # begin the thread
                
                self.open = True
                self.sig_portNotification.emit("Opened") # alert that the port is open
                self.sendCommand(cmds.OPEN) # confirm that the arduino is running our program by requesting a known response

    tic = 0 # for timing
    
    # callback on EMG packet passed through from the thread. Converts the single bytearray into two lists of unsigned int16 data [0,4095] = [0 V, 3.3 V]
    def emgDataReady(self, emg_array : bytearray):
        self.logger.debug(f"Time since last emg recv: {time.perf_counter() - self.tic}") # confirm real time running in log
        self.tic = time.perf_counter()
        bin_data = [] # create an array of paired byte values that form each emg sample
        for i in range(0, 100, 2):
            bin_data.append([emg_array[i], emg_array[i+1]])
        
        comb_data = [] # shift the first byte by 8 bits and OR with the second to get the 16-bit value of the sample
        for i in range(len(bin_data)):
            comb_data.append(bin_data[i][0] << 8 | bin_data[i][1])
            
        # split the data into those from sensor 1 and 2 (samples are stored alternately [1,2,1,2,etc] by the arduino)
        self.sensor_data_0 = []
        self.sensor_data_1 = [] 
        for i in range(0, len(comb_data), 2):
            self.sensor_data_0.append(comb_data[i])
            self.sensor_data_1.append(comb_data[i+1])
        
        self.sig_emgDataReady.emit([self.sensor_data_0, self.sensor_data_1]) # emit the data to the program
        
    # callback on IT packet passed through from the thread, converts to unsigned int16 values from bytearray
    def impTmpDataReady(self, imp_array : bytearray, temp_array : bytearray):
        self.imp_data = [] # as with EMG, shift the first byte by 8 bits and OR with the second byte
        for i in range(0, len(imp_array), 2):
            self.imp_data.append(imp_array[i] << 8 | imp_array[i+1])
        self.temp_data = [] # as with EMG, shift the first byte by 8 bits and OR with the second byte
        for i in range(0, len(temp_array), 2):
            self.temp_data.append(temp_array[i] << 8 | temp_array[i+1])
        
        self.sig_impTempReady.emit(self.imp_data, self.temp_data)
    
    # callback on thread finish, reset the polling timer and emit a signal to alert the port closed
    def threadFinished(self):
        if self.open:
            self.com_timer.start()
            self.sig_portNotification.emit("Closed")
            self.open = False
    
    # *UNUSED* manual close port. Unused as does not clear up the thread.
    # May be used by the main file to ensure we free the com port resource on program close
    def closePort(self):
        if self.open:
            self.serial_obj.close()
    
    # Callback on reciept of command signal from other widgets, sends the command to the serial thread
    def sendCommand(self, command):
        command_o = [60, 255, 62, 10] # 255 will be unused in scheme and ignored, 60 & 62 are start end markers for control "<" & ">"
        if command >= 255: # checks for valid commands
            self.logger.error("Serial control recieved command out of scope")        
        elif command > self.max_command:
            self.logger.error("Serial control recieved valid value but out of range")
            
        else:
            command_o[1] = command
            
        self.sig_sendCommand.emit(bytearray(command_o))
        
    # Callback on reciept of response to issued command. 
    def procCMDResponse(self, resp):
        # If the response is to our polling command emit a common port notification, if not emit the response to the other widgets to process
        if resp == "HI":
            self.sig_portNotification.emit("Arduino Connected")
        if resp == "N":
            self.sig_deviceNotification.emit("Sensors Disconnected")
        if resp == "Y":
            self.sig_deviceNotification.emit("Connected") 
        if resp == "1":
            self.sig_deviceNotification.emit("Sen 2 disconnected")
        if resp == "2":
            self.sig_deviceNotification.emit("Sen 1 disconnected")
        self.sig_cmdResponse.emit(resp)

    # if the serial port alerts an error and is closed, propagate this state, restart the polling timer, and end the thread
    def procSerialError(self):
        self.sig_serialError.emit()
        self.com_timer.start()
        self.serial_thread.quit()

# SerialObject class containing the serial port. Permits a way to move the Serial port onto a seperate thread to the UI
class SerialObject(QObject):

    sig_emgDataReady = pyqtSignal(bytearray) # signal emitted when an EMG packet is recieved
    sig_impAndTempDataReady = pyqtSignal(bytearray, bytearray) # signal emitted when an IT packet is recieved
    sig_cmdResponse = pyqtSignal(str) # signal emitted when a command response is recieved
    sig_serialError = pyqtSignal() # signal emitted if the Serial port has an error
    
    lastImp = None
    
    wait_for_response = False # Flag applied when a sent command expects a response from the Arduino
    
    def __init__(self, com_port_info, baud_rate, array_size):
        # initialise the serial port settings
        super(SerialObject, self).__init__()
        self.array_size = array_size
        self.logger = logging.getLogger("app_logger.SerialThread")
        self.baud_rate = baud_rate
        self.com_port_info = com_port_info
        self.data_queue = bytearray(self.array_size)
        
        # intiialise the serial port object based on the detected device
        self.serial_port = QSerialPort(self.com_port_info)
        self.serial_port.setBaudRate(self.baud_rate)
        self.serial_port.readyRead.connect(self.handleReadyRead) # signal for new data
        self.serial_port.errorOccurred.connect(self.comError) # signal when error occurs
        self.logger.info("Opening COM port")
        if not self.serial_port.open(QIODevice.ReadWrite): # open the port and check it doesn't fail
            self.logger.error("Port open failed! Error code: %s" % self.serial_port.error())
        else:
            self.serial_port.clear() # flush the buffer
            self.serial_port.setDataTerminalReady(True) # begin coms
        
    def close(self):
        self.logger.info("Closing COM port")
        self.serial_port.close()
    
    def handleReadyRead(self):
        while not self.serial_port.atEnd(): # read each byte until the input buffer is empty
            data_i = self.serial_port.read(1) # get one byte from the buffer
            del self.data_queue[0] # remove the first byte from our fifo
            self.data_queue.extend(data_i) # place the new byte on our fifo
            
            # parse the fifo buffer, search for the expected markers of an EMG, IMP or TMP packet, with appropriate space between the header and footer. If found, we know the byte inbetween form our data packet and we can emit these
            if self.data_queue[0:4] == bytearray(b"EMG:") and self.data_queue[-4:] == bytearray(b":GME"):
                self.sig_emgDataReady.emit(self.data_queue[4:-4])            
            elif self.data_queue[0:4] == bytearray(b"IMP:") and self.data_queue[20:24] == bytearray(b":PMI"):
                self.lastImp = self.data_queue[4:20]
            elif self.data_queue[0:4] == bytearray(b"TMP:") and self.data_queue[8:12] == bytearray(b":PMT"):
                self.sig_impAndTempDataReady.emit(self.lastImp, self.data_queue[4:8])
            
            if self.wait_for_response: 
                idx = self.data_queue.find(bytearray(b"REP:")) # if we know we are expecting a response, look for the REP header and footer to identify the packet
                if idx > -1:
                    idx_end = self.data_queue.find(bytearray(b":PER")) # as responses are variable size we must locate the end programatically rather than manually as before
                    if idx_end > -1:
                        response = self.data_queue[idx+4:idx_end].decode('utf-8') # take the data within. Responses are always strings, so decode with utf-8 to get the string meaning rather than a bytearray
                        print(f"response: {response}")
                        self.sig_cmdResponse.emit(response) # emit the response
                        self.wait_for_response = False
                        for i in range(idx_end+4):
                            self.data_queue[i] = 0 # delete the footer from the buffer so we don't find this response again
 
    # callback on reciept of a command from the other widgets. Writes the command to the serial port
    def sendCommand(self, command):
        if command[1] <= cmd_wait_response:
            self.wait_for_response = True
        if self.serial_port.write(command) < 1:
            self.logger.error(f"Command not written to Serial port, attempted command: {command}")
    

    # callback for error signal from com port
    def comError(self, error):
        if error == 0: #weird case where error callback occurs with no error?
            return
        self.logger.error(f"Serial Com error, code: {error}") # log the error
        self.sig_serialError.emit() # emit our own error signal to the program
        

# *Obsolete* - kept for posterity of one method to use QThread
class SerialThread(QThread):

    sig_emgDataReady = pyqtSignal(bytearray)
    sig_impAndTempDataReady = pyqtSignal(bytearray, bytearray)
    
    lastImp = None
    
    def __init__ (self, com_port_info, baud_rate, array_size):
    
        super(SerialThread, self).__init__()
        self.array_size = array_size
        self.logger = logging.getLogger("app_logger.SerialThread")
        self.baud_rate = baud_rate
        self.com_port_info = com_port_info
        self.data_queue = bytearray(self.array_size) #TODO: work out max size for command and data
        
    def __del__(self):
        self.wait() # TODO: check why this
        
    def run(self):
        self.serial_port = QSerialPort(self.com_port_info)
        self.serial_port.setBaudRate(self.baud_rate)
        self.serial_port.readyRead.connect(self.handleReadyRead, type=Qt.QueuedConnection)
        self.logger.info("Opening COM port")
        if not self.serial_port.open(QIODevice.ReadWrite):
            self.logger.error("Port open failed! Error code: %s" % self.serial_port.error())
        else:
            self.serial_port.clear()
            self.exec()
            
    def close(self):
        self.logger.info("Closing COM port")
        self.serial_port.close()
        
    # should we have a terminator, what is the possibility that EMG: or IMP: or TMP: appear within the EMG data itself?
    # ideally we should flush the data through once a packet is read, or use a state machine and a counter to avoid this
    # we also can have a rough idea of how often the data should arrive, and so can ignore if needed based on timers?
    def handleReadyRead(self):
        while not self.serial_port.atEnd():
            data_i = self.serial_port.read(1)
            del self.data_queue[0]
            self.data_queue.extend(data_i)
            #TODO: fix this to properly process the interface for all messages
            if self.data_queue[0:4] == bytearray(b"EMG:") and self.data_queue[-4:] == bytearray(b":EMG"):
                self.sig_emgDataReady.emit(self.data_queue[4:])
            elif self.data_queue[0:4] == bytearray(b"IMP:") and self.data_queue[20:24] == bytearray(b":IMP"):
                self.lastImp = self.data_queue[4:20]
            elif self.data_queue[0:4] == bytearray(b"TMP:") and self.data_queue[8:12]:
                self.sig_impAndTempDataReady.emit(self.lastImp, self.data_queue[4:8])
                
    def sendCommand(self, command):
        if self.serial_port.write(command) < 1:
            self.logger.error(f"Command not written to Serial port, attempted command: {command}")