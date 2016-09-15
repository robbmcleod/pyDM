# -*- coding: utf-8 -*-
"""
Testing code for pyDMPipeline

Created on Thu Jul 24 15:09:13 2014

@author: Robert A. McLeod
"""

import numpy as np
#import h5py
#from win32com.client import gencache
#import win32com.client
import time
import matplotlib.pyplot as plt
# import TEM
import pyDM
#import DM3lib
#import qimage2ndarray
#from PyQt4 import QtCore, QtGui

#temcomwrapper = gencache.EnsureModule('{BC0A2B03-10FF-11D3-AE00-00A024CBA50C}', 0, 1, 9)
#titansim = gencache.EnsureDispatch('TEMScripting.Instrument') # Equivalent to connecting to the TEM
#titantia = win32com.client.Dispatch("ESVision.Application")

""" For the most part using my TEM interface is easier here"""
#tem = TEM.TEM_FeiTitan()

def acquireImage(tx=1, binning=[1,1], framesize=[0,0,2048,2048], procmode=3 ):
        # Using the pyDM interface which is much faster than the FEI COM32-based interface
        # proc = 1 is unprocessed, proc = 2 is dark reference subtracted, proc = 3 is gain normalized image
        
        if np.size( binning ) == 1:
            binning = [binning, binning]
            
        # Clear queue (this is quite lame, I need a message handler)
        for J in np.arange(0, pyDM.cntMsg()):
            pyDM.getMessage()
        
        # acqparamstr = "set_acqparam_"+procmode+"_" + str(tx) + "_"+binx+"_"+biny+"_0_0_2048_2048"
        acqstr = "acquire_test_"+str(procmode)+"_" + str(tx)+"_"+str(binning[0])+"_"+str(binning[1])+"_"+str(framesize[0])+"_"+str(framesize[1])+"_"+str(framesize[2])+"_"+str(framesize[3])
        print( "Try: " + acqstr )
        pyDM.sendMessage(acqstr)
        while(True):
            # TO DO: add a timer in the C++
            if( pyDM.cntMsg() > 0):
                out = pyDM.getMessage()
                if out.split('_')[1] == "done" :
                    mage = pyDM.getImage( (framesize[2]-framesize[0])/binning[0], (framesize[3]-framesize[1])/binning[1], 2)
                    break
                else:
                    print( "Unrecognized message : " + out )
                    break
            time.sleep(0.001)
        return mage
        
    
pyDM.connect()
pyDM.sendMessage( "print_readoutparams")
#t0 = time.time()
#for J in np.arange(0,10):
#    testmage = acquireImage( tx=1, binning=[1,1], procmode=1 )
#t1 = time.time()
#print( "Average time (s) : " + str((t1-t0)/10) )
#
#plt.figure()
#plt.imshow( testmage )
#plt.show( block=False )

#t0 = time()
#for J in np.arange(0,100):
#    testmage = pipe.getImage()
#t1 = time()
#print( "mage shape" + str(testmage.shape)  )
#print( "Time per getImage (s) : " + str((t1-t0)/100) )

