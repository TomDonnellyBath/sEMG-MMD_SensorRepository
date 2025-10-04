# Widget indicating progress through the trial
# Sort of unnecessary, but felt a nice thing for the participant to have a vibe of how long is left
# Simply updates a progress bar after each of the 12 activations

import logging, re, csv, os, time
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *

class ProgressDisplayWidget(QWidget):
    
    def __init__(self, *args, **kwargs):
    
        super(ProgressDisplayWidget, self).__init__(*args, **kwargs)
        
        self.logger = logging.getLogger("app_logger.ProgressDisplayWidget")
        
        # very simple widget, consists of a description label and the bar itself
        self.logger.info("Setting up widgets.")
        self.lpb = QLabel("Task Progress: ")
        self.pb = QProgressBar()
        
        self.logger.info("Setting up signals.")
        
        
        # simple form layout such that progress bar takes as much space as it can after the descriptor
        self.logger.info("Setting up layout.")
        layout = QFormLayout()
        layout.addRow(self.lpb, self.pb)
        self.setLayout(layout)
        
        self.logger.info("Finalising.")
        
        self.pb.setValue(00) # intialise the progress bar as empty
        
    def postInit(self):
        pass
        
    def resetSoftware(self):
        pass
        
    #driven by controls, simply a decimal value of progress between 0 and 1
    # Query: why didn't we just send the multplied value? Original intent was likely to send a 1-12 value from controls and calc here...
    def progressUpdate(self, value):
        self.logger.info(f"Recieved val: {value}")
        self.pb.setValue(value*100)