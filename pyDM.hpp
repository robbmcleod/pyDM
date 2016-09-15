// Function will use a combination of mappable memory and a pipeline to communicate between Python and Digital Micrograph

//Python Libraries (should be called first for DEFINEs
#include <Python.h>

//C++ Libraries
#include <iostream>
#include <string> 

//Boost Libraries
//#include <boost/thread.hpp>
//#include <boost/thread/mutex.hpp>
#include <boost/interprocess/windows_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>
// Use re-written native Win32 message queue (Boost-style)
#include "message_queue.hpp"
#include <boost/tokenizer.hpp>
#include <boost/interprocess/detail/win32_api.hpp>

#include <boost/python.hpp>
#include <boost/numpy.hpp>

using namespace boost;
using namespace std;
using namespace boost::interprocess;

namespace p = boost::python;
namespace np = boost::numpy;

// Variable declarations
// Pipeline / Mappable Memory stuff
#define QUEUE_LENGTH 32
size_t const BUF_SIZE = 512;
#define IMAGE_CNT 2
#define IMAGE_SIZE 2048*2048*4 // Detector returns uint16 when unprocessing, int32 when processed.
#define MEM_SIZE IMAGE_SIZE * IMAGE_CNT
TCHAR tImageMap[] = TEXT("pyDMImageMap");
TCHAR tMQPytoDM[] = TEXT("mqPytoDM");
TCHAR tMQDMtoPy[] = TEXT("mqDMtoPy");
unsigned int priority = 0;

// Boost.Interprocess
boost::interprocess::mapped_region* mapImage;
message_queue* mqDMtoPy;
message_queue* mqPytoDM;



// Function declarations
char const* pyDM_version();
bool pyDM_connect();
np::ndarray pyDM_getImage( int x, int y, size_t bytedepth );
np::ndarray pyDM_getImageRaveled( int xy, size_t bytedepth );
p::str pyDM_getMessage();
p::object pyDM_checkMessageCount();
void pyDM_sendMessage( p::str tosend );
