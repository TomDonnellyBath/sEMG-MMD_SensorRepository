# File to store command enum such that multiple files can access these

from enum import IntEnum

cmds = IntEnum('cmds', ["OPEN", "CHECK_SEN", "IMP_TMP", "STOP_IMP_PER", "START_IMP_PER", "SET_AD_RANGE_1", "SET_AD_RANGE_2", "SET_AD_RANGE_3", "SET_AD_RANGE_4", "SET_AD_PGA_1", "SET_AD_PGA_5" ], start=0) # command details are given in the Arduino code
cmd_wait_response = cmds.IMP_TMP - 1 # Commands above this value do not receive a response from the Arduino and so we should not wait for them to return a value