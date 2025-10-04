# Widget to host program control buttons and execute the flow of a trial within the experiment
# Toggle of EMG display 
# Button to start next task
# Input for UserID
# Display for output file and current task 

import logging
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from PyQt5.QtMultimedia import QSound

import time

from enum import Enum

import csv

from Commands import cmds

State = Enum('State', ['INACTIVE', 'STIM_ON', 'STIM_OFF'])

TIME_OFF = 12000 # activity at rest time (12 secs), includes time for IT read
TIME_ON = 5000 # activity on time (5 secs)

tasks = [
    "None",
    "1.1", "1.2", "1.3", 
    "2.1", "2.2", "2.3",
    "3.1", "3.2", "3.3", "3.4",
    "4.1", "4.2", "4.3",
    "5.1", "5.2", "5.3", "5.4", "5.5", "5.6", "5.7", "5.8", "5.9",
    "Complete"
] 

tasks_file_friendly = [
    "None",
    "1_1", "1_2", "1_3", 
    "2_1", "2_2", "2_3",
    "3_1", "3_2", "3_3", "3_4",
    "4_1", "4_2", "4_3",
    "5_1", "5_2", "5_3", "5_4", "5_5", "5_6", "5_7", "5_8", "5_9",
    "Complete"
] # task file names using "_" to not make weird files strings. 5 contains spare trial numbers to offset if issues without restarting the program (i.e. keeps files in participant folder)

class ControlsWidget(QWidget):

    sig_resetStim = pyqtSignal() # signal to indicate a trial ended and the stim should be reset
    sig_setStimVal = pyqtSignal(int) # signal to indicate the stim image should update to image based on int
    sig_setStimOn = pyqtSignal() # signal to indicate the stim image should show active and green border
    sig_setStimOff = pyqtSignal() # signal to indicate the stim image should show active and red border
    sig_progressUpdate = pyqtSignal(float) # signal to update the progress bar, value between 0 and 1
    sig_toggleParticipantVisibility = pyqtSignal(int) # signal indicating whether EMG display is visible on main window (not needed when using participant specific window
    sig_sendCommand = pyqtSignal(int) # signal to send commands to the Arduino via serial com widget
    
    # initialise values
    stimVal = 1 # current stim value
    max_stimVal = 6 # max stim value
    repetition = 1 # current repetition value
    max_repetition = 2 # maximum repetition value
    p_m = max_stimVal*max_repetition # total counter of activity periods
    state = State.INACTIVE # state for trial state machine
    
    current_task = 0 # counter for trials
    
    save_initialised = False # without a save folder initialised, do not permit any recording
    
    results_dir = None # directory for storage
    enabled_recording = False # toggle to indicate recording allowed, i.e. whether a trial can be started (disable if periodic IT in progress)
    recording_file = None # current trial file name
    
    in_task = False # flag for whether a trial is in progress
    
    # storage for incoming IT variables
    imp_raw = None
    imp = None
    phase = None
    tmp = None

    polling = True
    
    debugging_save = False
    
    def __init__(self, *args, **kwargs):
    
        super(ControlsWidget, self).__init__(*args, **kwargs)
        
        self.logger = logging.getLogger("app_logger.ControlsWidget")
        
        # setup control widgets, check box for display toggle, buttons to control periodic IT and trial start, input for a participant ID (determines results folder name), label to show current task number 
        self.logger.info("Setting up widgets.")
        self.lte = QLabel("Toggle EMG Visibility")
        self.lpi = QLabel("Participant ID:")
        self.lct_t = QLabel("Current Task:")
        self.cbte = QCheckBox() 
        self.pbnt = QPushButton("Start Next Task")
        self.dbgpb = QPushButton("DEBUG!! DELETE IF YOU SEE THIS")
        self.lepi = QLineEdit("")
        self.lct = QLabel("None")
        self.sspb = QPushButton("Start/Stop ImpPoll")
        
        self.cbte.setChecked(True) # initialise as display visible
        
        # connect signals to local functions for above widget changes 
        self.logger.info("Setting up signals.")
        self.pbnt.pressed.connect(self.startNextTask)
        self.cbte.stateChanged.connect(self.cbStateChanged)
        self.lepi.textEdited.connect(self.lepiTextEdited)
        self.lepi.returnPressed.connect(self.lepiEditingFinished)
        self.sspb.pressed.connect(self.startStopImpPoll)
        self.dbgpb.pressed.connect(self.dbgpbPressed)
        
        # simple form layout
        self.logger.info("Setting up layout.")
        layout = QFormLayout()
        layout.addRow(self.lte, self.cbte)
        layout.addRow(self.sspb, self.pbnt) # change pbnt to dbgpb if debug required 
        layout.addRow(self.lpi, self.lepi)
        layout.addRow(self.lct_t, self.lct)
        
        self.setLayout(layout)
        
        self.logger.info("Finalising.")
        
        # initialise the input box for participant ID and limit inputs to only valid int between 1 and 100
        self.lepi.setText("0")
        intvalidator = QIntValidator(1,100)
        self.lepi.setValidator(intvalidator)
        
        # setup a timer for running 
        self.stim_timer = QTimer()
        self.stim_timer.setSingleShot(True)
        self.stim_timer.setTimerType(0)
        self.stim_timer.timeout.connect(self.processTask)
        
        # setup a timer for delaying the IT read after stimulus change to allow for participant adjustment
        self.stim_reaction_timer = QTimer()
        self.stim_reaction_timer.setSingleShot(True)
        self.stim_reaction_timer.setTimerType(0)
        self.stim_reaction_timer.timeout.connect(self.getImpAndTemp)
        
        # preload alert sounds 
        self.alert_on = QSound("assets/two_k.wav")
        self.alert_off = QSound("assets/one_k.wav")
    
    # initialise the state of the control fields, forcing the user to ensure host and sensors are connected before running any trials or commands
    def postInit(self):
        self.pbnt.setEnabled(False)
        self.sspb.setEnabled(False)
        self.lepi.setEnabled(False)
        self.dbgpb.setEnabled(False)
        
    # reset the control buttons such that a re-established connection must be ensured before continuing    
    def resetSoftware(self):
        if self.in_task: # reset current task back 1 if we lost connection during the task
            self.stim_timer.stop()
            self.stim_reaction_timer.stop()
            self.enabled_recording = False
            self.in_task = False
            self.current_task -= 1
            self.sig_resetStim.emit()
        self.pbnt.setEnabled(False) 
        
    # on sensors connected allow for PID to be input    
    def sensorsReady(self):
        self.lepi.setEnabled(True)
        
    # emit a signal to indicate hiding of EMG display    
    def cbStateChanged(self, state):
        self.sig_toggleParticipantVisibility.emit(state)
    
    # ensure start button not permitted until PID is locked
    def lepiTextEdited(self, text):
        self.pbnt.setEnabled(False)
        
    # used in a debugging environment which ignores certain program flow rules. The debug button is not currently instantiated
    def dbgpbPressed(self):
        self.debugging_save = not self.debugging_save
        self.enabled_recording = not self.enabled_recording
        self.pbnt.setEnabled(False)
    
    btn_action = None # used for storing warning response in lepi editing
    
    # function to check if the field is valid, and to check if data already exists for the input participant ID
    def lepiEditingFinished(self):
        if len(self.lepi.text()) > 0: # if valid input created a directory object
            dir = QDir()
            if not dir.exists("Results"): # ensure we have a results folder
                dir.mkdir("Results") 
            dir.cd("Results") # change to results folder
            if dir.exists("PID"+self.lepi.text()): # if PID# already exists in results, raise a warning before continuing
                self.logger.warning("Participant folder already exists")
                warning = QMessageBox()
                warning.setWindowTitle("Participant already exists!")
                warning.setIcon(QMessageBox.Warning)
                warning.setText(f"PID{self.lepi.text()} already exists, continue?")
                warning.setStandardButtons(QMessageBox.Yes|QMessageBox.No)
                warning.setDefaultButton(QMessageBox.No)
                warning.buttonClicked.connect(self.checkAction)
                warning.exec() # opens, processes clicked, then returns to here
                if self.btn_action == "&No": # if we don't want to overwrite, do nothing and leave the function, otherwise continuine with the accept routine
                    self.btn_action = None
                    return
                self.btn_action = None
            else:
                dir.mkdir("PID"+self.lepi.text())
            dir.cd("PID"+self.lepi.text())
            # set controls ready for recording, disable the PID field to prevent editing once running
            self.results_dir = dir
            self.save_initialised = True
            self.pbnt.setEnabled(True)
            self.sspb.setEnabled(True)
            self.lepi.setEnabled(False)
            self.dbgpb.setEnabled(True)
            self.sig_sendCommand.emit(cmds.STOP_IMP_PER) # disable the periodic IT read on the sensors
            self.polling = False
            
        else:
            self.pbnt.setEnabled(False)
        
    # callback for when the warning is responded to by the user, stores response
    def checkAction(self, button):
        self.btn_action = button.text()

    # callback function when start-stop button is pressed. Check if periodic IT polling is on or not, and send appropriate toggle command
    def startStopImpPoll(self):
        if self.polling:
            self.sig_sendCommand.emit(cmds.STOP_IMP_PER)
            self.pbnt.setEnabled(True)
            self.polling = False
        else:
            self.sig_sendCommand.emit(cmds.START_IMP_PER)
            self.pbnt.setEnabled(False)
            self.polling = True
    
    # call back function on start task button pressed
    def startNextTask(self):
        self.sig_sendCommand.emit(cmds.STOP_IMP_PER) # just to be sure, stop periodic (it shouldn't be running due to above preventing pbnt press while running)
        self.polling = False
        self.sspb.setEnabled(False) # disable start stop button
        self.in_task = True # flag we are in a trial
        self.current_task += 1 # update the trial counter
        self.lct.setText(tasks[self.current_task]) # update the trial information display
        self.pbnt.setEnabled(False) # prevents multiple presses
        self.sig_progressUpdate.emit(0) # reset progress bar
        # update stim counter and state, send signals to update the images on the stim display
        self.state = State.STIM_OFF 
        self.stimVal = 1
        self.enabled_recording = True
        self.sig_setStimVal.emit(self.stimVal)
        self.sig_setStimOff.emit()
        self.stim_timer.start(TIME_OFF) # start the stim timer for a rest period
        
        
    # call back function on stim timer finish
    def processTask(self):
        
        if self.state == State.STIM_OFF: # if in rest period
            if self.stimVal > self.max_stimVal: # check if we have completed the trial, reset variables if so 
                self.stimVal = 1
                self.sig_resetStim.emit() 
                self.enabled_recording = False
                self.pbnt.setEnabled(True)
                self.sspb.setEnabled(True)
                self.in_task = False
                return
            self.alert_on.play() # play the pickup alert
            self.state = State.STIM_ON # set state to activity period
            self.sig_setStimOn.emit() # update the stimulus display
            self.stim_timer.start(TIME_ON) # restart the timer for an activity period
            
       
        elif self.state == State.STIM_ON: # if in an activity period
            self.alert_off.play() # play the put down alert
            self.stim_reaction_timer.start(2000) # start a 2 second timer to allow for participant adjustment
            self.state = State.STIM_OFF # set state to rest period
            p_u = (self.stimVal-1)*self.max_repetition # create a progress update 
            p_u += self.repetition
            self.repetition +=1
            if self.repetition > self.max_repetition: # if we have done 2 repetitions of the grip
                self.repetition = 1
                self.stimVal += 1 
                self.sig_setStimVal.emit(self.stimVal) # send the updated grip value to the display 
            self.sig_setStimOff.emit() # reset the stim image to red border for rest
            self.sig_progressUpdate.emit(p_u/self.p_m) # update the progress bar
            self.stim_timer.start(TIME_OFF) # restart the timer for a rest period
        self.logger.info(f"Stim val = {self.stimVal}, {self.state}") # log the trial progress state
            
    # callback function from reaction timer to emit an IT read request command over the Serial Com widget
    def getImpAndTemp(self):
        self.sig_sendCommand.emit(cmds.IMP_TMP)     
                    
    # callback on receipt of new EMG data from the Arduino
    def newEMGData(self, data_i):
        if self.enabled_recording: # check if we are recording
            l = len(data_i[0])
            ts = [""]*l # setup a set of cells length of the data for the first coloumn
            ts[0] = QDateTime.currentDateTime().toString("yyyy-MM-dd hh-mm-ss-zzz") # initialise the first cell of this coloumn to contain a time stamp
            data = data_i
            data.insert(0,ts) # prepend the timestamp coloumn to the data list of lists (3 lists same length now)
            stim_state = [""]*l # create an "empty" list of length data for storing class of each sample
            if self.state == State.STIM_OFF: # check for rest or activity, then fill stim_state list with class value
                stim_state = ["0" for s in stim_state]
            else:
                stim_state = [self.stimVal for s in stim_state]
            data.append(stim_state) # append the class list (4 lists same length now TS, data1, data2, class)
            if self.imp != None: # check if we have outstanding IT data to save
                
                con_list = self.imp_raw + self.imp + self.phase + self.tmp # concatenate all the IT data
                len_con = len(con_list)
                for i in range(len_con): # create "empty" lists for each value in the concatenated list
                    a_ = [""]*l
                    a_[0] = con_list[i] # add each value to the first cell of each list
                    data.append(a_)
                del self.imp_raw # clear the recorded IT data
                del self.imp
                del self.phase
                del self.tmp
            data = [list(x) for x in zip(*data)] # transpose the list of lists so now we have a list of rows that can be put into a csv
            
            # save to appropriate csv file, open file with "a" to ensure we are appending not overwritting data
            if self.debugging_save: # check if we are doing a real or debug save
                with open(self.results_dir.absolutePath() + "/"+ "debugging" + ".csv", 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerows(data)
            else:
                with open(self.results_dir.absolutePath() + "/"+ tasks_file_friendly[self.current_task] + ".csv", 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerows(data)

    # callback function for new IT data
    def newImpAndTempData(self, imp_raw_i, imp_i, phase_i, tmp_i):
        if self.enabled_recording: # only store if in a trial
            if not (self.imp == None or self.tmp == None): # check and warn if we are not recording fast enough or have an issue clearing the old values
                self.logger.warning("Lost imp or tmp data to overwrite")
            # store the new IT data locally for save in above function
            self.imp_raw = imp_raw_i
            self.imp = imp_i
            self.phase = phase_i
            self.tmp = tmp_i