#include <iostream>

#include "vers.h"
#include "async_internal.h"

using namespace std::literals::string_literals;


int main(int argc, char const* argv[]) 
{
	using namespace otus_hw9;
	try
	{
		Options options;
		if (!options.parse_command_line(argc, argv))
			return 1;
		
		IProcessorPtr_t processor = otus_hw9::create_processor(options);
		processor->process();	
	}	
	catch(const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
