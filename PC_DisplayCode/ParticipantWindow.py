# Separate pop up window used to show the participant only the stimulus and the progress bar
# This way the participant is not influenced by the real time EMG display providing feedback regarding grip strength

import logging
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *

from ProgressDisplay import ProgressDisplayWidget
from StimulusDisplay import StimulusDisplayWidget

class ParticipantWindowWidget(QWidget):
    
    def __init__(self, *args, **kwargs):
    
        super(ParticipantWindowWidget, self).__init__(*args, **kwargs)
        self.setWindowFlags(Qt.WindowTitleHint | Qt.WindowMaximizeButtonHint) # grey out the minimise and close options on the window to prevent closing or hiding this by accident during experiment
        self.setWindowTitle("Participant Stimulus Display")
        
        self.logger = logging.getLogger("app_logger.ParticipantWindowWidget")
        
        # instantiate a stimulus and progress display widget
        self.logger.info("Setting up widgets.")
        self.sdw = StimulusDisplayWidget()
        self.pdw = ProgressDisplayWidget()
        
        # Signals for the sub widgets here are still setup by the MainWindow __init__ function 
        self.logger.info("Setting up signals.")
        
        
        self.logger.info("Setting up layout.")
        layout = QVBoxLayout()
        layout.addWidget(self.sdw, Qt.AlignCenter)
        layout.addWidget(self.pdw)
        self.setLayout(layout)
        
        self.logger.info("Finalising.")
        
    # pass on post init call    
    def postInit(self):
        self.pdw.postInit()
        self.sdw.postInit()
        
    def resetSoftware(self):
        pass