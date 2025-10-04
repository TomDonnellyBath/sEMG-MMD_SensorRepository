# Widget to host display information from the experiment rig excluding EMG
# Display of any warnings or errors detected on the COM bus
# Display of any warnings or errors sent by the Arduino Host relating to itself, or its sensor units
# Display of most recent sensor temperature and impedance readings 

import logging
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *

from Commands import cmds # import for enum of commands

import numpy as np
import sys

class UtilDisplayWidget(QWidget):

    sig_sensorsReady = pyqtSignal() # signal emitted when the Arduino alerts that sensors are detected on the bus
    sig_sendCommand = pyqtSignal(int) # signal emitted when a command must be sent to the Arduino 
    sig_impTempReady = pyqtSignal(list, list, list, list) # signal emitted with the latest processed impedance and temperature readings. Lists are "raw AD5933 values", "calculated magnitudes", "calculated phases", "calculated temperatures"

    FCU_temp = 0
    FCU_imp = [0,0]
    FCU_phase = [0,0]
    ECR_temp = 0
    ECR_imp = [0,0]
    ECR_phase = [0,0]
    
    def __init__(self, *args, **kwargs):
    
        super(UtilDisplayWidget, self).__init__(*args, **kwargs)
        
        self.logger = logging.getLogger("app_logger.UtilDisplayWidget")
        
        # setup widgets for this widget, contains a series of labels.
        self.logger.info("Setting up widgets.")
        self.lcb = QLabel() # displays com port info
        self.lsd = QLabel() # displays sensor info
        self.lti = QLabel() # displays latest imp and temp data
        self.lcb_t = QLabel("COM Port Info:")
        self.lsd_t = QLabel("Sensors Info: ")
        self.lti_t = QLabel("Current Values: ")
        
        # initialise values to unknown. \u03A9 is ohm, \u00B0 is degree
        self.lcb.setText("Unknown State")
        self.lsd.setText("Unknown State")
        self.lti.setText(f"FCU Temp: {self.FCU_temp}, FCU Imp Sen 1: {int(self.FCU_imp[0])}\u03A9, {int(self.FCU_phase[0])}\u00B0 "
                         f"FCU Imp Sen 2: {int(self.FCU_imp[1])}\u03A9, {int(self.FCU_phase[1])}\u00B0\n"
                         f"ECR Temp: {self.ECR_temp}, ECR Imp Sen 1: {int(self.ECR_imp[0])}\u03A9, {int(self.ECR_phase[0])}\u00B0 "
                         f"ECR Imp Sen 2: {int(self.ECR_imp[1])}\u03A9, {int(self.ECR_phase[1])}\u00B0")
        
        self.logger.info("Setting up signals.")
        
        # simple form layout, rows are ordered vertically top to bottom as added, ensures consistent spacing coloumn spacing for all rows of the form
        self.logger.info("Setting up layout.")
        layout = QFormLayout()
        layout.addRow(self.lcb_t, self.lcb)
        layout.addRow(self.lsd_t, self.lsd)
        layout.addRow(self.lti_t, self.lti)
        
        self.setLayout(layout)
        
        self.logger.info("Finalising.")
        
        # force a size policy such that if the when the impedance values are replaced by real ones (longer) the whole display does not jump around
        sp = QSizePolicy()
        sp.setRetainSizeWhenHidden(True)
        sp.setHorizontalPolicy(4)
        sp.setVerticalPolicy(4)
        self.lti.setSizePolicy(sp)
        
        # set up a timer for 1 second, which on timeout emits the command to ask the arduino to confirm the sensor precense
        self.poll_sen_timer = QTimer()
        self.poll_sen_timer.setInterval(1000)
        self.poll_sen_timer.timeout.connect(self.check_for_sensors)
        
        # set up a timer for half a second. Used to flash the port label on disconnect
        self.timer = QTimer()
        self.timer.setSingleShot(True) # do not automatically reset the timer
        self.timer.setInterval(500)
        self.timer.timeout.connect(self.resetLabel)
        
        self.flash_count = 10
        
    def postInit(self):
        pass
        
    def resetSoftware(self):
        pass
        
    # If the serial widget emits an error, alert that the connection is lost and flash the label to draw attention to this    
    def serialError(self):
        self.lsd.setText("Unknown State")
        self.lcb.setText("Connection Lost")
        self.lcb.setStyleSheet("QLabel { background-color : orange;}")
        self.timer.start()
        
    # Function used to flash the label 10 times    
    def resetLabel(self):
        self.flash_count -= 1
        
        if self.flash_count < 0:
            self.flash_count = 10
        elif self.flash_count % 2 != 0:
            self.lcb.setStyleSheet("QLabel { background-color : orange;}")
            self.timer.start()
        else:
            self.lcb.setStyleSheet("QLabel {}")
            self.timer.start()
        
    # Updates the serial label based on Serial widget detection of the Arduino. If the arduino is connected, begin the polling timer for the sensors    
    def setComNotification(self, noti):
        self.lcb.setText(noti)
        if noti == "Arduino Connected":
            self.poll_sen_timer.start()
            
    # Updates the sensor label based on the response of the Arduino through the serial widget. If the sensors are both there, end the polling and emit the ready signal, which unlocks the program for recoring
    def setDeviceNotification(self, noti):
        self.lsd.setText(noti)
        if noti == "Connected":
            self.poll_sen_timer.stop()
            self.sig_sensorsReady.emit()
            
    # Function to process the raw impedance and temperature data        
    def setImpTempData(self, imp, temp):
        self.ECR_temp = temp[1] * 0.00390625 # the MAX30205 provides this value as a multiplier for the recorded interger value. Performing float maths on the PC is more straightforward so done here
        self.FCU_temp = temp[0] * 0.00390625
        
        # Convert the raw readings to signed intergers 
        as_i16 = []
        for i in range(len(imp)):
            as_i16.append(int.from_bytes(imp[i].to_bytes(2, byteorder=sys.byteorder, signed=False), byteorder=sys.byteorder, signed=True))
            
        # calculate the magnitude and phase values as per the AD5933 datasheet    
        comb_val = []
        phase_val = []
        for i in range(0, len(as_i16), 2):
            comb_val.append(np.sqrt(as_i16[i]**2 + as_i16[i+1]**2))
            phase_val.append(np.rad2deg(np.arctan2(as_i16[i+1], as_i16[i])))
        
        # apply the previously calculated polynomial fit for each sensor to the recorded magnitude value
        self.FCU_imp = np.polyval([2.08553599726588e-06, 14.1943911679110, 39.1817314267489], [comb_val[0], comb_val[1]])
        self.ECR_imp = np.polyval([7.95978542134756e-07, 14.1353065689958, 67.5674420365175], [comb_val[2], comb_val[3]])
        
        # apply the open (no DUT) adjustment in phase to the recorded phase
        self.FCU_phase = [96.991226597164020 - i for i in phase_val[:2]]
        self.ECR_phase = [95.3347889128904 - i for i in phase_val[2:]]

        # update the label with the new values of temperature and impedance for each sensor
        self.lti.setText(f"FCU Temp: {self.FCU_temp}, FCU Imp Sen 1: {int(self.FCU_imp[0])}\u03A9, {int(self.FCU_phase[0])}\u00B0 "
                         f"FCU Imp Sen 2: {int(self.FCU_imp[1])}\u03A9, {int(self.FCU_phase[1])}\u00B0\n"
                         f"ECR Temp: {self.ECR_temp}, ECR Imp Sen 1: {int(self.ECR_imp[0])}\u03A9, {int(self.ECR_phase[0])}\u00B0 "
                         f"ECR Imp Sen 2: {int(self.ECR_imp[1])}\u03A9, {int(self.ECR_phase[1])}\u00B0")
        
        # store this data in the log for reference and prior testing
        self.logger.debug(f"{imp}")
        self.logger.info(f"FCU Temp: {self.FCU_temp}, FCU Imp Sen 1: {int(self.FCU_imp[0])}, {int(self.FCU_phase[0])} "
                         f"FCU Imp Sen 2: {int(self.FCU_imp[1])}, {int(self.FCU_phase[1])}    "
                         f"ECR Temp: {self.ECR_temp}, ECR Imp Sen 1: {int(self.ECR_imp[0])}, {int(self.ECR_phase[0])} "
                         f"ECR Imp Sen 2: {int(self.ECR_imp[1])}, {int(self.ECR_phase[1])}")
        
        # emit a signal indicating the conversion is complete and that the new data can be saved by the control widget (saves raw impedance data also)
        self.sig_impTempReady.emit(imp, self.FCU_imp.tolist() + self.ECR_imp.tolist(), self.FCU_phase + self.ECR_phase, [self.FCU_temp, self.ECR_temp])
    
    # function to store signal emit command on timer finish for checking sensor state on arduino
    def check_for_sensors(self):
        self.sig_sendCommand.emit(cmds.CHECK_SEN)
        
        