# Widget to host information from the EMG sensors
# Display of EMG over time, shows previous samples updating from right to left

import logging
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
import numpy as np

import pyqtgraph as pg

import time

class EMGDisplayWidget(QWidget):

    
    def __init__(self, packet_size, max_packets, *args, **kwargs):
    
        super(EMGDisplayWidget, self).__init__(*args, **kwargs)
        
        self.logger = logging.getLogger("app_logger.EMGDisplayWidget")
        
        # initialise values based on expected packets and maximum packets
        self.packet_size = packet_size//2
        self.max_packets = max_packets
        self.max_size = packet_size * max_packets
        self.num_graphs = 2
        
        # data and graph storage
        self.display_data = [[], []] # a fifo buffer for incoming data packets
        self.graphs = []
        self.line_refs = []
        
        
        self.logger.info("Setting up widgets.")
        
        # setup both graphs to contain a plot widget from pyqtgraph (using matplotlib was too slow to update, pyqtgraph doesn't introduce delays into the program)
        for i in range(self.num_graphs):
            self.graphs.append(pg.PlotWidget())
            self.graphs[i].setYRange(0, 4096, padding=0.025) # force the range so this doesn't dynamically update based on min and max plotted values

            
        
        self.logger.info("Setting up signals.")
        
        self.logger.info("Setting up layout.")
        layout = QVBoxLayout()
        for graph in self.graphs:
            layout.addWidget(graph)
        
        self.setLayout(layout)
        
        self.logger.info("Finalising.")
        
        self.displayClear()    
        
        # force a policy such that if the EMG is toggled hidden on the main window it is able to reclaim its spot on return
        sp = QSizePolicy()
        sp.setRetainSizeWhenHidden(True)
        sp.setHorizontalPolicy(4)
        sp.setVerticalPolicy(4)
        self.setSizePolicy(sp)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        
        
    def postInit(self):
        pass
        
    def resetSoftware(self):
        pass
        
    def sensorsReady(self):
        pass
    
    # wipes all display data and initialises each fifo with 0s (probably should be 2048 for pretty reasons)
    def displayClear(self):
        self.display_data = [[], []] # clear buffers
        for i in range(self.max_packets): # refill buffers
            self.display_data[0].append(np.zeros(self.packet_size).tolist())
            self.display_data[1].append(np.zeros(self.packet_size).tolist())
        
        # update graphs with cleared buffers
        if len(self.line_refs) == 0:
            for i in range(self.num_graphs):
                ref  = self.graphs[i].plot(np.zeros(self.max_size).tolist())
                self.line_refs.append(ref)
        else:
            for i in range(self.num_graphs):
                self.line_refs[i].setData(np.zeros(self.max_size).tolist())
    
    tic = 0
    # called on receipt of new data from the serial com
    def insertNewData(self, data):
        """
        toc = time.perf_counter() # used for testing timing of updates when matplotlib seemed laggy
        print(toc - self.tic)
        self.tic = toc
        """
        for i in range(self.num_graphs):
            self.display_data[i].append(data[i]) # append the new data packet
        self.displayUpdate()
        
    def displayUpdate(self):
        for i in range(self.num_graphs):
            if len(self.display_data[i]) > self.max_packets:
                self.display_data[i].pop(0) # remove the oldest data packet
            data = []
            for j in range(len(self.display_data[i])):
                data.extend(self.display_data[i][j]) # combine all the packets into one list
            self.line_refs[i].setData(data) # plot