# Window to host all Widgets for the GUI
# Controls routing of all signals withing the program
# Ensures the program has warnings to prevent accidental early closure before experiment is complete, or all data is not saved

import logging
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *

from Controls import ControlsWidget
from EMGDisplay import EMGDisplayWidget
from ProgressDisplay import ProgressDisplayWidget
from SerialCom import SerialComWidget
from StimulusDisplay import StimulusDisplayWidget
from ParticipantWindow import ParticipantWindowWidget
from UtilDisplay import UtilDisplayWidget

from time import sleep

class MainWindow(QMainWindow):

    packet_size = 50 # defines the size of the expeted EMG packet from the Arduino host board
    max_packets = 200 # defines the maximum number of packets for display on the real time display
    
    def __init__(self, *args, **kwargs):
    
        super(MainWindow, self).__init__(*args, **kwargs)
        
        self.setWindowTitle("MMD Experiment Software")
        
        self.logger = logging.getLogger("app_logger.MainWindow")
        
        # setup all widget used in the program, assign to an array for iteration access
        self.logger.info("Setting up widgets.")
        self.cw  = ControlsWidget()
        self.edw = EMGDisplayWidget(self.packet_size, self.max_packets)
        self.pdw = ProgressDisplayWidget()
        self.scw = SerialComWidget(self.packet_size)
        self.sdw = StimulusDisplayWidget()
        self.udw = UtilDisplayWidget()
        self.pww = ParticipantWindowWidget()
        
        self.widgets_l = [self.cw, self.edw, self.pdw, self.scw, self.sdw, self.udw, self.pww]
        
        # setup all signals between the widgets. These primarily are sourced from the control widget to indicate updates during the trial, or from the Serial Com widget sending data or command responses. More detail on signals provided in signal source widgets.
        self.logger.info("Setting up signals.")
        # control widget signals
        self.cw.sig_resetStim.connect(self.sdw.resetStim) 
        self.cw.sig_resetStim.connect(self.pww.sdw.resetStim)
        self.cw.sig_progressUpdate.connect(self.pdw.progressUpdate)
        self.cw.sig_progressUpdate.connect(self.pww.pdw.progressUpdate)
        self.cw.sig_sendCommand.connect(self.scw.sendCommand)
        self.cw.sig_setStimOff.connect(self.sdw.setStimOff)
        self.cw.sig_setStimOff.connect(self.pww.sdw.setStimOff)
        self.cw.sig_setStimOn.connect(self.sdw.setStimOn)
        self.cw.sig_setStimOn.connect(self.pww.sdw.setStimOn)
        self.cw.sig_setStimVal.connect(self.sdw.setStimVal)
        self.cw.sig_setStimVal.connect(self.pww.sdw.setStimVal)
        self.cw.sig_toggleParticipantVisibility.connect(self.edw.setVisible)
        
        # serial com widget signals
        self.scw.sig_emgDataReady.connect(self.cw.newEMGData)
        self.scw.sig_emgDataReady.connect(self.edw.insertNewData)
        self.scw.sig_impTempReady.connect(self.udw.setImpTempData)
        self.scw.sig_deviceNotification.connect(self.udw.setDeviceNotification)
        self.scw.sig_portNotification.connect(self.udw.setComNotification)
        self.scw.sig_serialError.connect(self.udw.serialError)
        
        # utility display widget signals. In these cases data from the serial port is processed as part of the utility display before it is sent on for saving or alternate display
        self.udw.sig_sensorsReady.connect(self.cw.sensorsReady)
        self.udw.sig_sensorsReady.connect(self.edw.sensorsReady)
        self.udw.sig_sendCommand.connect(self.scw.sendCommand)
        self.udw.sig_impTempReady.connect(self.cw.newImpAndTempData)
        
        
        # simple layout management to assemble the final screen as observed
        self.logger.info("Setting up layout.")
        layout_t = QHBoxLayout()
        layout_t.addWidget(self.sdw)
        layout_t.addWidget(self.edw)
        widget_t = QWidget()
        widget_t.setLayout(layout_t) # top: put the stimulus and emg displays side by side 
        
        layout_b = QHBoxLayout()
        layout_b.addWidget(self.udw)
        layout_b.addWidget(self.cw)
        widget_b = QWidget()
        widget_b.setLayout(layout_b) # bottom: put the utils display (impedance, port conection info) and the controls side by side
        
        layout = QVBoxLayout()
        layout.addWidget(widget_t)
        layout.addWidget(self.pdw)
        layout.addWidget(widget_b) # sandwich the progress display widget between the top and bottom layouts
        
        widget = QWidget()
        widget.setLayout(layout)
        
        self.logger.info("Finalising.")
        self.setCentralWidget(widget)
 
        # call postInit on all wdigets which allows for any setup that is reliant on knowledge of other widgets instantiated in the program
        for w in self.widgets_l:
            w.postInit()
        
        # maximise the participant window (reduced layout) and centre on screen
        self.pww.showMaximized()
        centre = QDesktopWidget().availableGeometry().center() 
        rect = self.frameGeometry()
        rect.moveCenter(centre)
        self.move(rect.topLeft())
        
    # pass through function for a reset state    
    def resetSoftware(self):
        for w in self.widgets_l:
            w.resetSoftware()
        
    # Override close action on X press to provide a check requiring confirmation before closing the program
    def closeEvent(self, evnt):
        self.evnt = evnt
        warning = QMessageBox()
        warning.setWindowTitle("Exit?")
        warning.setIcon(QMessageBox.Warning)
        warning.setText("Exit program?")
        warning.setStandardButtons(QMessageBox.Yes|QMessageBox.No)
        warning.setDefaultButton(QMessageBox.No)
        warning.buttonClicked.connect(self.closeCatchAction)
        warning.setWindowFlags(Qt.Dialog | Qt.WindowTitleHint | Qt.CustomizeWindowHint)
        warning.exec()

    # check which button is pressed during the closeEvent warning message check
    def closeCatchAction(self, button):
        if button.text() == "&Yes":
            
            self.scw.closePort()
            sleep(0.1) # leave time for close down actions
            self.pww.close()
            super(MainWindow, self).closeEvent(self.evnt)
        else:
            self.evnt.ignore()
            
        
# As I have named this MainWindow it could be confusing, this just throws information in case the wrong file is called 
if __name__ == "__main__":
    print("The programm cannot be stated by calling the MainWindow directly, please run \"python main.py\"")