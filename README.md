# pyDM

Python - Gatan Microscopy Suite interface

For Digital Micrograph 2.0+ 
(there is a version for 1.7 as well, ask if interested)

Author: Robert A. McLeod
Email: robbmcleod@gmail.com

Requires Microsoft Visual Studio 2005 and the Gatan API ver. 2. Builds a plugin that must be placed into the 
Gatan Digital Micrograph plugins folder, and a .pyd file which may be loaded as a Python module. Data is 
transferred between the two softwares by a shared mapped memory region. For many image dimensions this plugin 
is faster than acquisition within Digital Micrograph with the proprietary .s scripting interface.

Compiled copies of the DLLs are found in Win32/Release.  

Installation (by hand):

pyDMPipeline.dll -> \ProgramData\Gatan\Plugins
boost_python-vc90-mt-1_52.dll -> <PYTHONPATH>
pyDM.pyb -> <PYTHONPATH>

See the pyDMPipeline.pdf for detailed notes.

Sample vcproj files are provided for Visual Studio 2005 (which is required).  Also a relatively older version of Boost
is required.  See the 




