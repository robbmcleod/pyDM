// This .cpp file encloses functions that are shared between both pyDM (the Python side) 
// and pyDMPipeline (the Digital Micrograph side).  Principally, this is the configuration .ini file.

// Hrm, maybe I want pyDMPipeline to be sent commands to create maps and such so that Python can change things?  
// That might be a lot smarter.
#include "stdafx.h"

// I do need to setup some default stuff in an .ini file?  Like the size of the message queues?
// Default settings that the user can modify would be nice in any case.  But do it in Python?
//#include <boost/property_tree/ptree.hpp>
//#include <boost/property_tree/ini_parser.hpp>

// This class encapsulates a boost.interprocess.mapped_region, with handles to tell it how big the associated image should be, and
// get/set commands to handle memcpy in a relatively safe fashion.

class pyDMSharedMage
{

};
