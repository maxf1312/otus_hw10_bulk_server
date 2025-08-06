#include <iostream>
#include <boost/asio.hpp>

#include "vers.h"
#include "bulkserver_utils.h"
#include "bulkserver_internal.h"
#include "async.h"

using namespace std::literals::string_literals;

int main(int argc, char const* argv[]) 
{
	using namespace otus_hw10;
	try
	{
		Options options;
		if (!options.parse_command_line(argc, argv))
			return 1;
		
		ba::io_context io_context;
	    async_server server(io_context, options.port, options.cmd_chunk_sz);
		io_context.run();
	}	
	catch(const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
