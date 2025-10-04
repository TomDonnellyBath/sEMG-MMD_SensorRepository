# Widget for displaying stimulus to the participant
# Shows the current grip to be performed using images shown to the participant during familiarisation
# When alternating whether the grip should be performed or not a green and red border are shown around the image. This in incorporated in the image file (ON/OFF)

import logging
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *

class StimulusDisplayWidget(QWidget):

    stims_off = None
    stims_on = None
    
    def __init__(self, *args, **kwargs):
    
        super(StimulusDisplayWidget, self).__init__(*args, **kwargs)
        
        self.logger = logging.getLogger("app_logger.StimulusDisplayWidget")
        
        self.logger.info("Setting up widgets.")
        self.image = QPixmap("assets\Stimulus.png")
        self.rest_image = QPixmap(r"assets\Neutral_COLOUR.png")
        
        # Preload the png files and assign them to arrays in order of performance
        
        self.stim_one_off = QPixmap(r"assets\LargeDiameterOff.png")
        self.stim_one_on = QPixmap(r"assets\LargeDiameterOn.png")
        self.stim_two_off = QPixmap(r"assets\PowerSphereOff.png")
        self.stim_two_on = QPixmap(r"assets\PowerSphereOn.png")
        self.stim_three_off = QPixmap(r"assets\PrecisionSphereOff.png")
        self.stim_three_on = QPixmap(r"assets\PrecisionSphereOn.png")
        self.stim_four_off = QPixmap(r"assets\MediumWrapOff.png")
        self.stim_four_on = QPixmap(r"assets\MediumWrapOn.png")
        self.stim_five_off = QPixmap(r"assets\ExtendedIndexFingerOff.png")
        self.stim_five_on = QPixmap(r"assets\ExtendedIndexFingerOn.png")
        self.stim_six_off = QPixmap(r"assets\AbductedThumbOff.png")
        self.stim_six_on = QPixmap(r"assets\AbductedThumbOn.png")
        self.stim_seven_off = QPixmap(r"assets\Neutral_COLOUR.png")
        self.stim_seven_on = QPixmap(r"assets\Neutral_COLOUR.png")
        
        self.stims_off = [self.stim_one_off, self.stim_two_off, self.stim_three_off, self.stim_four_off, self.stim_five_off, self.stim_six_off, self.stim_seven_off]
        self.stims_on = [self.stim_one_on, self.stim_two_on, self.stim_three_on, self.stim_four_on, self.stim_five_on, self.stim_six_on, self.stim_seven_on]
        
        self.current_stim = 1 # variable to store the current grip
        
        
        # create a label with which to display the image
        self.l = QLabel()
        
        self.w = self.l.width()
        self.h = self.l.height()
        
        self.resetStim() # set a neutral image prior to experiment start
        
        
        self.logger.info("Setting up signals.")
        
        
        self.logger.info("Setting up layout.")
        layout = QVBoxLayout()
        layout.addWidget(self.l)
        self.setLayout(layout)
        
        self.logger.info("Finalising.")
        
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.l.setAlignment(Qt.AlignCenter)
        
    # common function following the init call of all widgets
    def postInit(self):
        pass
        
    # common function run if the software is reset    
    def resetSoftware(self):
        pass
        
    # update the image to the green bordered version of the current grip
    def setStimOn(self):
        if self.stims_on is not None:
            self.currentPixmap = self.stims_on[self.current_stim-1]
            self.l.setPixmap(self.currentPixmap.scaled(self.w, self.h, Qt.KeepAspectRatio))
        else:
            self.logger.warning(f"Recieved stim on {self.current_stim} but has no image")
        
    # update the image to the red bordered version of the current grip
    def setStimOff(self):
        if self.stims_on is not None:
            self.currentPixmap = self.stims_off[self.current_stim-1]
            self.l.setPixmap(self.currentPixmap.scaled(self.w, self.h, Qt.KeepAspectRatio))
        else:
            self.logger.warning(f"Recieved stim off {self.current_stim} but has no image")
        
    # updates the current grip 
    def setStimVal(self, stim_val):
        self.current_stim = stim_val
        
    # set the stimulus image back to the neutral display, occurs between trials    
    def resetStim(self):
        self.currentPixmap = self.rest_image
        self.l.setPixmap(self.currentPixmap.scaled(self.w, self.h, Qt.KeepAspectRatio))
        
    # override an internal QT event; keeps the widget a consistent size when the image is changed
    def resizeEvent(self, rEvnt):
        self.w = self.l.width()
        self.h = self.l.height()
        self.l.setPixmap(self.currentPixmap.scaled(self.w, self.h, Qt.KeepAspectRatio))
    