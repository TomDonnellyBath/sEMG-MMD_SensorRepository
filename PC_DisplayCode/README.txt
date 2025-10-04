This folder contains the code to run the PC software used for recording data from the EMG sensors, used to create the sEMG-MMD.

For future projects, adjustments may be necessary to the Serial setup to adapt the device IDs for recognising a different Arduino device to that used in this project. [productIdentifier() and vendorIndentifier() checks on line 69 of SerialCom.py]

The requirements.txt file provides the exact environment used when this code was run, some packages may be surplus and unused, as the environment was generally used by me for all QT based projects.