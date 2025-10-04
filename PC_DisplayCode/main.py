# -*- coding: utf-8 -*-
"""
Created on Wed Jun 21 2023

@author: tommd
"""

# core file, run this to begin the program
# handles the initial window and logger set up

import sys
import logging
from PyQt5.QtWidgets import QApplication
from PyQt5.QtCore import QDateTime, QDir
from MainWindow import MainWindow

logger = logging.getLogger("app_logger") # setup a logger, each widget creates a new input to the logger, the argument passed is used to show in the log where the message comes from
logger.setLevel(logging.DEBUG)

# check logs folder, if we have more than 100 logs, clear the oldest
dir = QDir()
if not dir.exists("Logs"):
    dir.mkdir("Logs")
dir.cd("Logs")
logs = dir.entryList()
l_logs = len(logs)
if l_logs > 100:
    for i in range(100, l_logs-1):
        dir.remove(logs[l_logs-i])
        
# Determine the output file name of the log
fh = logging.FileHandler("Logs/log_%s.log" % QDateTime.currentDateTime().toString("yyyy-MM-dd hh-mm-ss"))
fh.setLevel(logging.DEBUG)

# setup the format of the logger, posts the time, the widget name, the level of message, and the message
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
fh.setFormatter(formatter)
logger.addHandler(fh)        

logger.info('creating QApp')
app = QApplication(sys.argv) # begin an app

logger.info('Attaching MainWindow to App')
window = MainWindow()
window.show() # show the app

logger.info('Executing event loop')
app.exec_() # run, starts the QT main loop